/* The GPUI-shaped GPU backend for Paint.h; see paintgpu.h for what it is,
   how it is turned on, and what it is still short of.

   The frame is one instance buffer and as few draws as the content allows.
   Every rounded rect, border, ellipse, glyph, image and gradient is 96 bytes
   in that buffer — bounds, colour, radius, border width, kind, content mask,
   uv — and the pixel shader evaluates the shape analytically, which is what
   `directx_renderer.rs` and Blade do and what D2D does not let us do. A batch
   is only broken when the pipeline or a bound texture has to change: an
   image, a path, or the end of the frame.

   Three pipelines:

   - quads. Four vertices per instance from SV_VertexID, instance data read
     out of a StructuredBuffer, shape and coverage from an SDF. This is nearly
     everything a frame draws.
   - triangles. Explicit vertices with a colour and a content mask, for the
     two things a quad cannot be: a stroke expanded on the CPU, and the
     stencil pass of a path fill.
   - stencil-and-cover for path fills, which is how you fill an arbitrary
     path on a GPU without a tessellator: draw the triangle fan of every
     contour into the stencil buffer with INVERT (even-odd) or with wrapping
     increment and decrement (nonzero), then draw the path's bounding box
     through a stencil test and let it write the colour. The fan is allowed
     to self-overlap, which is the whole point — no library has to untangle
     the outline first.

   Antialiasing comes from two places. Quads and glyphs carry their own: the
   SDF gives analytic coverage, and a glyph is a coverage texture. Paths and
   strokes have no analytic form here, so they get theirs from the sample
   count — GPUI_PAINT_MSAA, 4 by default. That is the one real architectural
   difference from Blade, which renders paths to an antialiased mask instead;
   MSAA is a prototype's version of the same answer, and having it as a knob
   is what makes its cost visible.

   Known gaps, which are why this is not the default: no subpixel glyph
   positioning — x is snapped, where DirectWrite positions at a third of a
   pixel — dashes are expanded on the CPU for lines and ignored on rounded
   rects, and the shaders are compiled with D3DCompile at startup rather than
   built to bytecode by fxc. */

#include "gpui/paintgpu.h"
#include "gpui/scene.h"

#if GPUI_OS_WINDOWS

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dwrite.h>
#include <dxgi1_2.h>
#include <math.h>

namespace gpui {

// GPUI_PAINT=gpu, read once. Anything else — unset, "d2d", nonsense — leaves
// the Direct2D backend in place.
bool PaintGpuOn() {
    static int on = -1;
    if (on < 0) {
        char buf[16] = {};
        DWORD n = GetEnvironmentVariableA("GPUI_PAINT", buf, sizeof(buf));
        on = (n > 0 && n < sizeof(buf) && StrCmpI(buf, "gpu") == 0) ? 1 : 0;
        if (on) {
            logf("paint: GPU backend (GPUI_PAINT=gpu)");
        }
    }
    return on == 1;
}

int PaintGpuSamples() {
    static int n = -1;
    if (n < 0) {
        char buf[16] = {};
        DWORD got =
            GetEnvironmentVariableA("GPUI_PAINT_MSAA", buf, sizeof(buf));
        n = 4;
        if (got > 0 && got < sizeof(buf)) {
            int v = atoi(buf);
            if (v == 1 || v == 2 || v == 4 || v == 8) {
                n = v;
            }
        }
    }
    return n;
}

namespace gpuw {

template <typename T>
static void Rel(T** p) {
    if (p && *p) {
        (*p)->Release();
        *p = nullptr;
    }
}

// ─── the shaders ─────────────────────────────────────────────────────────

// Kinds, matching the switch in PSQuad.
enum {
    kQuadRect = 0,    // rounded rect fill; radius 0 is a plain rect
    kQuadBorder = 1,  // the same outline, `border` wide, inside the edge
    kQuadEllipse = 2, // fill
    kQuadEllipseB = 3,
    kQuadGlyph = 4,    // coverage out of the atlas
    kQuadImage = 5,    // premultiplied BGRA out of the bound image
    kQuadGradient = 6, // linear, between two points in DIP space
    kQuadSolid = 7     // no shaping at all; the cover pass of a path fill
};

static const char* kShaderSrc = R"HLSL(
cbuffer Globals : register(b0) {
    float2 uViewport;
    float2 uPad;
};

struct Inst {
    float4 rect;   // x, y, w, h in DIPs
    float4 color;  // straight rgba
    float4 misc;   // radius, border, kind, unused
    float4 clip;   // x0, y0, x1, y1 — GPUI's content mask
    float4 uv;     // atlas rect, or the two gradient endpoints
    float4 color2; // the gradient's far end
};

StructuredBuffer<Inst> gInst : register(t0);
Texture2D<float4> gAtlas : register(t1);
Texture2D<float4> gImage : register(t2);
SamplerState gSamp : register(s0);

struct VSOut {
    float4 pos    : SV_Position;
    float2 local  : TEXCOORD0;
    float2 halfsz : TEXCOORD1;
    float4 color  : COLOR0;
    float4 color2 : COLOR1;
    float4 misc   : TEXCOORD2;
    float4 clipr  : TEXCOORD3;
    float2 uv     : TEXCOORD4;
    float2 gpos   : TEXCOORD5;
    float4 uvraw  : TEXCOORD6;
};

VSOut VSQuad(uint vid : SV_VertexID, uint iid : SV_InstanceID) {
    Inst i = gInst[iid];
    float2 corner = float2((vid == 1 || vid == 3) ? 1.0 : 0.0,
                           (vid >= 2) ? 1.0 : 0.0);
    float2 p = i.rect.xy + corner * i.rect.zw;
    VSOut o;
    o.pos = float4(p.x / uViewport.x * 2.0 - 1.0,
                   1.0 - p.y / uViewport.y * 2.0, 0.0, 1.0);
    o.halfsz = i.rect.zw * 0.5;
    o.local = p - (i.rect.xy + o.halfsz);
    o.color = i.color;
    o.color2 = i.color2;
    o.misc = i.misc;
    o.clipr = i.clip;
    o.uv = lerp(i.uv.xy, i.uv.zw, corner);
    o.uvraw = i.uv;
    o.gpos = p;
    return o;
}

// iq's rounded box: negative inside, and in the same units as the position,
// which here are DIPs and therefore pixels.
float sdRound(float2 p, float2 b, float r) {
    float2 q = abs(p) - b + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

float4 PSQuad(VSOut v) : SV_Target {
    if (v.gpos.x < v.clipr.x || v.gpos.x > v.clipr.z ||
        v.gpos.y < v.clipr.y || v.gpos.y > v.clipr.w) {
        discard;
    }
    int kind = (int)(v.misc.z + 0.5);
    if (kind == 5) {
        // Already premultiplied: WIC decoded it that way.
        float4 t = gImage.Sample(gSamp, v.uv) * v.color.a;
        if (t.a <= 0.0) {
            discard;
        }
        return t;
    }
    float4 col = v.color;
    float cov = 1.0;
    if (kind == 0 || kind == 1) {
        float r = min(v.misc.x, min(v.halfsz.x, v.halfsz.y));
        float d = sdRound(v.local, v.halfsz, r);
        float outer = saturate(0.5 - d);
        cov = (kind == 0) ? outer : outer - saturate(0.5 - (d + v.misc.y));
    } else if (kind == 2 || kind == 3) {
        float2 rr = max(v.halfsz, 1e-4);
        // Close enough to a distance for an antialiased edge, and exact on a
        // circle, which is what almost every one of these is.
        float d = (length(v.local / rr) - 1.0) * min(rr.x, rr.y);
        float outer = saturate(0.5 - d);
        cov = (kind == 2) ? outer : outer - saturate(0.5 - (d + v.misc.y));
    } else if (kind == 4) {
        cov = gAtlas.Sample(gSamp, v.uv).r;
    } else if (kind == 6) {
        float2 d = v.uvraw.zw - v.uvraw.xy;
        float t = saturate(dot(v.gpos - v.uvraw.xy, d) / max(dot(d, d), 1e-6));
        col = lerp(v.color, v.color2, t);
    }
    float a = col.a * cov;
    if (a <= 0.0) {
        discard;
    }
    return float4(col.rgb * a, a);
}

// ─── triangles ───────────────────────────────────────────────────────────

struct TriIn {
    float2 pos   : POSITION;
    float4 color : COLOR;
    float4 clipr : TEXCOORD0;
};

struct TriOut {
    float4 pos   : SV_Position;
    float4 color : COLOR;
    float4 clipr : TEXCOORD0;
    float2 gpos  : TEXCOORD1;
};

TriOut VSTri(TriIn i) {
    TriOut o;
    o.pos = float4(i.pos.x / uViewport.x * 2.0 - 1.0,
                   1.0 - i.pos.y / uViewport.y * 2.0, 0.0, 1.0);
    o.color = i.color;
    o.clipr = i.clipr;
    o.gpos = i.pos;
    return o;
}

float4 PSTri(TriOut v) : SV_Target {
    if (v.gpos.x < v.clipr.x || v.gpos.x > v.clipr.z ||
        v.gpos.y < v.clipr.y || v.gpos.y > v.clipr.w) {
        discard;
    }
    return float4(v.color.rgb * v.color.a, v.color.a);
}
)HLSL";

// ─── instance and vertex data ────────────────────────────────────────────

struct Inst {
    float rect[4];
    float color[4];
    float misc[4];
    float clip[4];
    float uv[4];
    float color2[4];
};

struct TriVert {
    float x, y;
    float color[4];
    float clip[4];
};

static void SetColor(float* out, Rgba c) {
    out[0] = c.r / 255.f;
    out[1] = c.g / 255.f;
    out[2] = c.b / 255.f;
    out[3] = c.a / 255.f;
}

// ─── the glyph atlas ─────────────────────────────────────────────────────
//
// One R8 texture, filled by a shelf allocator: a glyph goes on the current
// row until it will not fit, then a new row starts under the tallest of the
// last. Good enough for text that is nearly all one or two sizes, which is
// what a UI is. A full atlas is cleared and refilled rather than grown.

constexpr int kAtlasDim = 1024;

struct GlyphKey {
    void* face = nullptr;
    uint32_t glyph = 0;
    // The em size in 1/4 px, so 13.5 and 14 are different entries.
    uint32_t size4 = 0;
};

struct GlyphEntry {
    GlyphKey key;
    bool used = false;
    // Where it landed, and where it sits relative to the pen.
    int x = 0, y = 0, w = 0, h = 0;
    int bearingX = 0, bearingY = 0;
};

constexpr int kGlyphSlots = 4096; // power of two, open addressed

struct Atlas {
    ID3D11Texture2D* tex = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    int penX = 0;
    int penY = 0;
    int rowH = 0;
    GlyphEntry slots[kGlyphSlots] = {};
};

static uint32_t GlyphHash(const GlyphKey& k) {
    uint64_t h = (uint64_t)(uintptr_t)k.face * 0x9e3779b97f4a7c15ull;
    h ^= (uint64_t)k.glyph * 0xc2b2ae3d27d4eb4full;
    h ^= (uint64_t)k.size4 * 0x165667b19e3779f9ull;
    h ^= h >> 29;
    return (uint32_t)(h & (kGlyphSlots - 1));
}

// ─── paths ───────────────────────────────────────────────────────────────

// Paint.h's `Path` is opaque, and the D2D backend defines it. This is the
// GPU backend's own; the entry points take and return the opaque one and
// cast, which is what P() below is for.
struct GpuPath {
    // Every contour flattened to a polyline, laid end to end, with `starts`
    // saying where each begins. Curves are flattened as they arrive, which is
    // what a GPU wants and what the D2D backend does not have to do.
    Vec<float> pts; // x, y pairs
    Vec<int> starts;
    bool winding = true;
    bool open = false;
    float minX = 1e30f, minY = 1e30f, maxX = -1e30f, maxY = -1e30f;
};

// ─── the target ──────────────────────────────────────────────────────────

struct GpuTarget {
    HWND hwnd = nullptr;
    IDXGISwapChain1* swap = nullptr;
    ID3D11Texture2D* backTex = nullptr;
    ID3D11RenderTargetView* backRtv = nullptr;
    // The multisampled surface everything is drawn into, resolved into the
    // back buffer at the end of the frame. Null when the sample count is 1,
    // and then the back buffer is drawn into directly.
    ID3D11Texture2D* msaaTex = nullptr;
    ID3D11RenderTargetView* msaaRtv = nullptr;
    ID3D11Texture2D* dsTex = nullptr;
    ID3D11DepthStencilView* dsv = nullptr;
    // The offscreen half: a plain texture plus the staging copy the pixels
    // are read back through.
    ID3D11Texture2D* offTex = nullptr;
    ID3D11RenderTargetView* offRtv = nullptr;
    ID3D11Texture2D* stage = nullptr;
    bool offscreen = false;
    int pxW = 0;
    int pxH = 0;
    int samples = 1;
};

// ─── process-wide GPU state ──────────────────────────────────────────────

struct Gpu {
    ID3D11Device* dev = nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    IDXGIFactory2* factory = nullptr;
    IDWriteFactory* dwrite = nullptr;

    ID3D11VertexShader* vsQuad = nullptr;
    ID3D11PixelShader* psQuad = nullptr;
    ID3D11VertexShader* vsTri = nullptr;
    ID3D11PixelShader* psTri = nullptr;
    ID3D11InputLayout* triLayout = nullptr;

    ID3D11Buffer* cb = nullptr;
    ID3D11Buffer* instBuf = nullptr;
    ID3D11ShaderResourceView* instSrv = nullptr;
    int instCap = 0;
    ID3D11Buffer* triBuf = nullptr;
    int triCap = 0;

    ID3D11BlendState* blend = nullptr;
    ID3D11RasterizerState* raster = nullptr;
    ID3D11RasterizerState* rasterMsaa = nullptr;
    ID3D11SamplerState* samp = nullptr;
    // Off; even-odd (invert); nonzero (two-sided wrap); cover-and-clear.
    ID3D11DepthStencilState* dsOff = nullptr;
    ID3D11DepthStencilState* dsEvenOdd = nullptr;
    ID3D11DepthStencilState* dsNonZero = nullptr;
    ID3D11DepthStencilState* dsCover = nullptr;

    Atlas atlas;
    // A 1x1 white texture, so the image slot is always bound to something.
    ID3D11ShaderResourceView* white = nullptr;

    bool ready = false;
};

static Gpu gGpu;

// What is being accumulated right now. Painter order is preserved by
// flushing whenever the pipeline or a bound texture has to change, so the
// batch is only ever the run of primitives that can go out together.
struct Batch {
    Vec<Inst> insts;
    Vec<TriVert> tris;
    ID3D11ShaderResourceView* image = nullptr;
    GpuTarget* target = nullptr;
    // The content mask, as a stack. GPUI carries one per primitive rather
    // than setting a scissor, which is what lets a clip change cost nothing.
    Vec<float> clipStack; // x0, y0, x1, y1 per entry
    float clip[4] = {0, 0, 1e6f, 1e6f};
    FrameStats stats;
};

static Batch gB;
static FrameStats gLastStats;

const FrameStats& LastFrameStats() {
    return gLastStats;
}

// ─── device setup ────────────────────────────────────────────────────────

static bool CompileShader(const char* entry, const char* target,
                          ID3DBlob** out) {
    ID3DBlob* err = nullptr;
    UINT flags =
        D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_WARNINGS_ARE_ERRORS;
    HRESULT hr =
        D3DCompile(kShaderSrc, strlen(kShaderSrc), "gpui.hlsl", nullptr,
                   nullptr, entry, target, flags, 0, out, &err);
    if (FAILED(hr)) {
        if (err) {
            logf("paint/gpu: %s failed: %s", Str(entry),
                 Str((const char*)err->GetBufferPointer()));
            err->Release();
        } else {
            logf("paint/gpu: %s failed %08x", Str(entry), (unsigned)hr);
        }
        return false;
    }
    if (err) {
        err->Release();
    }
    return true;
}

static bool MakeAtlas(Gpu* g) {
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = kAtlasDim;
    td.Height = kAtlasDim;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(g->dev->CreateTexture2D(&td, nullptr, &g->atlas.tex))) {
        return false;
    }
    return SUCCEEDED(
        g->dev->CreateShaderResourceView(g->atlas.tex, nullptr, &g->atlas.srv));
}

static bool MakeWhite(Gpu* g) {
    uint32_t px = 0xffffffff;
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = 1;
    td.Height = 1;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA sd = {};
    sd.pSysMem = &px;
    sd.SysMemPitch = 4;
    ID3D11Texture2D* tex = nullptr;
    if (FAILED(g->dev->CreateTexture2D(&td, &sd, &tex))) {
        return false;
    }
    HRESULT hr = g->dev->CreateShaderResourceView(tex, nullptr, &g->white);
    tex->Release();
    return SUCCEEDED(hr);
}

// The four stencil states the path fill needs. The colour write is turned
// off for the two that only mark coverage; the cover state tests what they
// wrote and zeroes it on the way past, so the buffer is clean for the next
// path without a clear.
static bool MakeStencilStates(Gpu* g) {
    D3D11_DEPTH_STENCIL_DESC d = {};
    d.DepthEnable = FALSE;
    d.StencilEnable = FALSE;
    if (FAILED(g->dev->CreateDepthStencilState(&d, &g->dsOff))) {
        return false;
    }

    d.StencilEnable = TRUE;
    d.StencilReadMask = 0xff;
    d.StencilWriteMask = 0xff;
    D3D11_DEPTH_STENCILOP_DESC inv = {};
    inv.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    inv.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    inv.StencilPassOp = D3D11_STENCIL_OP_INVERT;
    inv.StencilFunc = D3D11_COMPARISON_ALWAYS;
    d.FrontFace = inv;
    d.BackFace = inv;
    if (FAILED(g->dev->CreateDepthStencilState(&d, &g->dsEvenOdd))) {
        return false;
    }

    D3D11_DEPTH_STENCILOP_DESC incr = inv, decr = inv;
    incr.StencilPassOp = D3D11_STENCIL_OP_INCR;
    decr.StencilPassOp = D3D11_STENCIL_OP_DECR;
    d.FrontFace = incr;
    d.BackFace = decr;
    if (FAILED(g->dev->CreateDepthStencilState(&d, &g->dsNonZero))) {
        return false;
    }

    D3D11_DEPTH_STENCILOP_DESC cover = {};
    cover.StencilFailOp = D3D11_STENCIL_OP_ZERO;
    cover.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
    cover.StencilPassOp = D3D11_STENCIL_OP_ZERO;
    cover.StencilFunc = D3D11_COMPARISON_NOT_EQUAL;
    d.FrontFace = cover;
    d.BackFace = cover;
    return SUCCEEDED(g->dev->CreateDepthStencilState(&d, &g->dsCover));
}

static bool EnsureGpu(PaintApp* pa) {
    Gpu* g = &gGpu;
    if (g->ready) {
        return true;
    }
    g->dev = (ID3D11Device*)PaintSharedD3dDevice(pa);
    g->factory = (IDXGIFactory2*)PaintSharedDxgiFactory(pa);
    g->dwrite = (IDWriteFactory*)PaintSharedDwrite(pa);
    if (!g->dev || !g->factory || !g->dwrite) {
        return false;
    }
    g->dev->GetImmediateContext(&g->ctx);
    if (!g->ctx) {
        return false;
    }

    ID3DBlob* vsq = nullptr;
    ID3DBlob* psq = nullptr;
    ID3DBlob* vst = nullptr;
    ID3DBlob* pst = nullptr;
    bool ok = CompileShader("VSQuad", "vs_5_0", &vsq) &&
              CompileShader("PSQuad", "ps_5_0", &psq) &&
              CompileShader("VSTri", "vs_5_0", &vst) &&
              CompileShader("PSTri", "ps_5_0", &pst);
    if (ok) {
        ok = SUCCEEDED(g->dev->CreateVertexShader(vsq->GetBufferPointer(),
                                                  vsq->GetBufferSize(), nullptr,
                                                  &g->vsQuad)) &&
             SUCCEEDED(g->dev->CreatePixelShader(psq->GetBufferPointer(),
                                                 psq->GetBufferSize(), nullptr,
                                                 &g->psQuad)) &&
             SUCCEEDED(g->dev->CreateVertexShader(vst->GetBufferPointer(),
                                                  vst->GetBufferSize(), nullptr,
                                                  &g->vsTri)) &&
             SUCCEEDED(g->dev->CreatePixelShader(pst->GetBufferPointer(),
                                                 pst->GetBufferSize(), nullptr,
                                                 &g->psTri));
    }
    if (ok) {
        D3D11_INPUT_ELEMENT_DESC el[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
             D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8,
             D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24,
             D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        ok = SUCCEEDED(g->dev->CreateInputLayout(el, 3, vst->GetBufferPointer(),
                                                 vst->GetBufferSize(),
                                                 &g->triLayout));
    }
    if (vsq) {
        vsq->Release();
    }
    if (psq) {
        psq->Release();
    }
    if (vst) {
        vst->Release();
    }
    if (pst) {
        pst->Release();
    }
    if (!ok) {
        return false;
    }

    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth = 16;
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(g->dev->CreateBuffer(&cbd, nullptr, &g->cb))) {
        return false;
    }

    // Premultiplied source, which is what both pixel shaders emit.
    D3D11_BLEND_DESC bd = {};
    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(g->dev->CreateBlendState(&bd, &g->blend))) {
        return false;
    }

    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    // No culling: a path's stencil fan is deliberately wound both ways, and
    // the nonzero rule reads the winding out of the front/back stencil ops
    // rather than out of a cull.
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    if (FAILED(g->dev->CreateRasterizerState(&rd, &g->raster))) {
        return false;
    }
    rd.MultisampleEnable = TRUE;
    if (FAILED(g->dev->CreateRasterizerState(&rd, &g->rasterMsaa))) {
        return false;
    }

    D3D11_SAMPLER_DESC sd = {};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(g->dev->CreateSamplerState(&sd, &g->samp))) {
        return false;
    }

    if (!MakeStencilStates(g) || !MakeAtlas(g) || !MakeWhite(g)) {
        return false;
    }
    g->ready = true;
    return true;
}

// The instance buffer, grown to hold `n`. A StructuredBuffer rather than a
// vertex buffer so the vertex shader can index it by SV_InstanceID and the
// quad needs no vertex data at all.
static bool EnsureInstBuf(Gpu* g, int n) {
    if (g->instCap >= n && g->instBuf) {
        return true;
    }
    int cap = g->instCap > 0 ? g->instCap : 4096;
    while (cap < n) {
        cap *= 2;
    }
    Rel(&g->instSrv);
    Rel(&g->instBuf);
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = (UINT)(cap * sizeof(Inst));
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bd.StructureByteStride = sizeof(Inst);
    if (FAILED(g->dev->CreateBuffer(&bd, nullptr, &g->instBuf))) {
        return false;
    }
    D3D11_SHADER_RESOURCE_VIEW_DESC sv = {};
    sv.Format = DXGI_FORMAT_UNKNOWN;
    sv.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    sv.Buffer.NumElements = (UINT)cap;
    if (FAILED(g->dev
                   ->CreateShaderResourceView(g->instBuf, &sv, &g->instSrv))) {
        return false;
    }
    g->instCap = cap;
    return true;
}

static bool EnsureTriBuf(Gpu* g, int n) {
    if (g->triCap >= n && g->triBuf) {
        return true;
    }
    int cap = g->triCap > 0 ? g->triCap : 4096;
    while (cap < n) {
        cap *= 2;
    }
    Rel(&g->triBuf);
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = (UINT)(cap * sizeof(TriVert));
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(g->dev->CreateBuffer(&bd, nullptr, &g->triBuf))) {
        return false;
    }
    g->triCap = cap;
    return true;
}

// ─── drawing ─────────────────────────────────────────────────────────────

static GpuTarget* T(PaintCtx* ctx) {
    return ctx ? (GpuTarget*)ctx->rt : nullptr;
}

static void SetViewportCb(Gpu* g, float w, float h) {
    D3D11_MAPPED_SUBRESOURCE m = {};
    if (SUCCEEDED(g->ctx->Map(g->cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
        float v[4] = {w, h, 0, 0};
        memcpy(m.pData, v, sizeof(v));
        g->ctx->Unmap(g->cb, 0);
    }
}

static void FlushQuads();
static void FlushTris(D3D11_PRIMITIVE_TOPOLOGY topo, bool colorWrite,
                      ID3D11DepthStencilState* ds);

// Everything queued, in order. Called before anything that would change what
// a draw would do, and at the end of the frame.
static void Flush() {
    FlushQuads();
    FlushTris(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true, gGpu.dsOff);
}

static void FlushQuads() {
    Gpu* g = &gGpu;
    if (gB.insts.len == 0 || !gB.target) {
        return;
    }
    if (!EnsureInstBuf(g, gB.insts.len)) {
        gB.insts.len = 0;
        return;
    }
    D3D11_MAPPED_SUBRESOURCE m = {};
    if (FAILED(g->ctx->Map(g->instBuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
        gB.insts.len = 0;
        return;
    }
    memcpy(m.pData, gB.insts.els, (size_t)gB.insts.len * sizeof(Inst));
    g->ctx->Unmap(g->instBuf, 0);

    ID3D11ShaderResourceView* srvs[3] = {g->instSrv, g->atlas.srv,
                                         gB.image ? gB.image : g->white};
    g->ctx->VSSetShader(g->vsQuad, nullptr, 0);
    g->ctx->PSSetShader(g->psQuad, nullptr, 0);
    g->ctx->VSSetShaderResources(0, 3, srvs);
    g->ctx->PSSetShaderResources(0, 3, srvs);
    g->ctx->IASetInputLayout(nullptr);
    g->ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    g->ctx->OMSetDepthStencilState(g->dsOff, 0);
    g->ctx->DrawInstanced(4, (UINT)gB.insts.len, 0, 0);

    gB.stats.instances += gB.insts.len;
    gB.stats.draws++;
    gB.insts.len = 0;
}

static void FlushTris(D3D11_PRIMITIVE_TOPOLOGY topo, bool colorWrite,
                      ID3D11DepthStencilState* ds) {
    Gpu* g = &gGpu;
    if (gB.tris.len == 0 || !gB.target) {
        return;
    }
    if (!EnsureTriBuf(g, gB.tris.len)) {
        gB.tris.len = 0;
        return;
    }
    D3D11_MAPPED_SUBRESOURCE m = {};
    if (FAILED(g->ctx->Map(g->triBuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
        gB.tris.len = 0;
        return;
    }
    memcpy(m.pData, gB.tris.els, (size_t)gB.tris.len * sizeof(TriVert));
    g->ctx->Unmap(g->triBuf, 0);

    UINT stride = sizeof(TriVert);
    UINT off = 0;
    g->ctx->VSSetShader(g->vsTri, nullptr, 0);
    g->ctx->PSSetShader(colorWrite ? g->psTri : nullptr, nullptr, 0);
    g->ctx->IASetInputLayout(g->triLayout);
    g->ctx->IASetVertexBuffers(0, 1, &g->triBuf, &stride, &off);
    g->ctx->IASetPrimitiveTopology(topo);
    g->ctx->OMSetDepthStencilState(ds, 0);
    g->ctx->Draw((UINT)gB.tris.len, 0);

    gB.stats.pathTriangles += gB.tris.len / 3;
    gB.stats.draws++;
    gB.tris.len = 0;
    g->ctx->OMSetDepthStencilState(g->dsOff, 0);
    if (!colorWrite) {
        g->ctx->PSSetShader(g->psTri, nullptr, 0);
    }
}

// Painter order is the whole contract: the tree draws back to front, so
// whichever of the two buffers is holding something has to go out before the
// other one starts filling. Every entry point declares which pipeline it is
// about to use.
static void EnsureQuadPhase() {
    if (gB.tris.len > 0) {
        FlushTris(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true, gGpu.dsOff);
    }
}

static void EnsureTriPhase() {
    if (gB.insts.len > 0) {
        FlushQuads();
    }
}

static void Push(const Inst& i) {
    EnsureQuadPhase();
    gB.insts.Append(i);
}

// The one place a primitive is made, so the content mask and the opacity in
// force are applied in exactly one place, the way GPUI applies them as the
// primitive is handed to the backend.
static void Quad(PaintCtx* ctx, int kind, float x, float y, float w, float h,
                 Rgba c, float radius = 0, float border = 0) {
    if (!T(ctx) || w <= 0 || h <= 0) {
        return;
    }
    c = PaintFade(ctx, c);
    if (c.a == 0) {
        return;
    }
    Inst i = {};
    i.rect[0] = x;
    i.rect[1] = y;
    i.rect[2] = w;
    i.rect[3] = h;
    SetColor(i.color, c);
    i.misc[0] = radius;
    i.misc[1] = border;
    i.misc[2] = (float)kind;
    memcpy(i.clip, gB.clip, sizeof(i.clip));
    Push(i);
}

static void TriVertex(float x, float y, Rgba c) {
    EnsureTriPhase();
    TriVert v = {};
    v.x = x;
    v.y = y;
    SetColor(v.color, c);
    memcpy(v.clip, gB.clip, sizeof(v.clip));
    gB.tris.Append(v);
}

// ─── target lifecycle ────────────────────────────────────────────────────

static void FreeSurfaces(GpuTarget* t) {
    Rel(&t->backRtv);
    Rel(&t->backTex);
    Rel(&t->msaaRtv);
    Rel(&t->msaaTex);
    Rel(&t->dsv);
    Rel(&t->dsTex);
}

void PaintTargetFree(PaintCtx* ctx) {
    GpuTarget* t = T(ctx);
    if (!t) {
        return;
    }
    FreeSurfaces(t);
    Rel(&t->offRtv);
    Rel(&t->offTex);
    Rel(&t->stage);
    Rel(&t->swap);
    if (gB.target == t) {
        gB.target = nullptr;
    }
    delete t;
    ctx->rt = nullptr;
}

// The multisampled colour surface and the stencil beside it. Both are
// remade on a resize, and both are skipped at one sample, where the back
// buffer is drawn into directly.
static bool MakeRenderSurfaces(Gpu* g, GpuTarget* t) {
    Rel(&t->msaaRtv);
    Rel(&t->msaaTex);
    Rel(&t->dsv);
    Rel(&t->dsTex);

    UINT quality = 0;
    int samples = t->samples;
    while (samples > 1) {
        if (SUCCEEDED(g->dev->CheckMultisampleQualityLevels(
                DXGI_FORMAT_B8G8R8A8_UNORM, (UINT)samples, &quality)) &&
            quality > 0) {
            break;
        }
        samples /= 2;
    }
    t->samples = samples < 1 ? 1 : samples;

    if (t->samples > 1) {
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = (UINT)t->pxW;
        td.Height = (UINT)t->pxH;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        td.SampleDesc.Count = (UINT)t->samples;
        td.SampleDesc.Quality = 0;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_RENDER_TARGET;
        if (FAILED(g->dev->CreateTexture2D(&td, nullptr, &t->msaaTex)) ||
            FAILED(g->dev->CreateRenderTargetView(t->msaaTex, nullptr,
                                                  &t->msaaRtv))) {
            return false;
        }
    }

    D3D11_TEXTURE2D_DESC dd = {};
    dd.Width = (UINT)t->pxW;
    dd.Height = (UINT)t->pxH;
    dd.MipLevels = 1;
    dd.ArraySize = 1;
    dd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dd.SampleDesc.Count = (UINT)t->samples;
    dd.SampleDesc.Quality = 0;
    dd.Usage = D3D11_USAGE_DEFAULT;
    dd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(g->dev->CreateTexture2D(&dd, nullptr, &t->dsTex)) ||
        FAILED(g->dev->CreateDepthStencilView(t->dsTex, nullptr, &t->dsv))) {
        return false;
    }
    // The only clear it gets: from here on the cover pass keeps it at zero.
    g->ctx->ClearDepthStencilView(t->dsv, D3D11_CLEAR_STENCIL, 1.f, 0);
    return true;
}

static bool BindBack(Gpu* g, GpuTarget* t) {
    Rel(&t->backRtv);
    Rel(&t->backTex);
    if (FAILED(t->swap->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                  (void**)&t->backTex))) {
        return false;
    }
    return SUCCEEDED(
        g->dev->CreateRenderTargetView(t->backTex, nullptr, &t->backRtv));
}

// Bind whichever surface this frame draws into, set the viewport and the
// pipeline state that does not change inside a frame.
static void BeginFrameState(Gpu* g, GpuTarget* t) {
    ID3D11RenderTargetView* rtv =
        t->offscreen ? t->offRtv : (t->msaaRtv ? t->msaaRtv : t->backRtv);
    g->ctx->OMSetRenderTargets(1, &rtv, t->dsv);
    D3D11_VIEWPORT vp = {};
    vp.Width = (float)t->pxW;
    vp.Height = (float)t->pxH;
    vp.MaxDepth = 1.f;
    g->ctx->RSSetViewports(1, &vp);
    g->ctx->RSSetState(t->samples > 1 ? g->rasterMsaa : g->raster);
    float bf[4] = {0, 0, 0, 0};
    g->ctx->OMSetBlendState(g->blend, bf, 0xffffffff);
    g->ctx->OMSetDepthStencilState(g->dsOff, 0);
    g->ctx->PSSetSamplers(0, 1, &g->samp);
    g->ctx->VSSetConstantBuffers(0, 1, &g->cb);
    g->ctx->PSSetConstantBuffers(0, 1, &g->cb);
    SetViewportCb(g, (float)t->pxW, (float)t->pxH);
    // The stencil is not cleared per frame. It is cleared once, when the
    // surface is made, and every path's cover pass zeroes what its own
    // stencil pass wrote — the cover quad is the path's bounding box, which
    // is by definition everything the fan could have touched. A full-surface
    // D24S8 clear every frame is pure bandwidth, and on a light scene it was
    // most of what the frame cost.
    gB.target = t;
    gB.image = nullptr;
    gB.insts.len = 0;
    gB.tris.len = 0;
    gB.clipStack.len = 0;
    gB.clip[0] = 0;
    gB.clip[1] = 0;
    gB.clip[2] = (float)t->pxW;
    gB.clip[3] = (float)t->pxH;
    gB.stats = FrameStats{};
}

bool PaintTargetBegin(PaintCtx* ctx, void* native, int pxW, int pxH) {
    if (!ctx || !ctx->pa || !EnsureGpu(ctx->pa)) {
        return false;
    }
    HWND hwnd = (HWND)native;
    if (!hwnd || pxW <= 0 || pxH <= 0) {
        return false;
    }
    Gpu* g = &gGpu;
    GpuTarget* t = T(ctx);
    if (t && (t->hwnd != hwnd || t->offscreen)) {
        gpuw::PaintTargetFree(ctx);
        t = nullptr;
    }
    if (!t) {
        t = new GpuTarget();
        t->hwnd = hwnd;
        t->pxW = pxW;
        t->pxH = pxH;
        t->samples = PaintGpuSamples();
        // The same chain the D2D path presents through — three buffers,
        // flip-sequential — so the two are compared on the same terms and a
        // screenshot still comes off the redirection surface.
        DXGI_SWAP_CHAIN_DESC1 desc = {};
        desc.Width = (UINT)pxW;
        desc.Height = (UINT)pxH;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 3;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        if (FAILED(g->factory->CreateSwapChainForHwnd(
                g->dev, hwnd, &desc, nullptr, nullptr, &t->swap))) {
            delete t;
            return false;
        }
        g->factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
        if (!BindBack(g, t) || !MakeRenderSurfaces(g, t)) {
            FreeSurfaces(t);
            Rel(&t->swap);
            delete t;
            return false;
        }
        ctx->rt = (PaintTarget*)t;
    } else if (t->pxW != pxW || t->pxH != pxH) {
        FreeSurfaces(t);
        t->pxW = pxW;
        t->pxH = pxH;
        if (FAILED(t->swap->ResizeBuffers(0, (UINT)pxW, (UINT)pxH,
                                          DXGI_FORMAT_UNKNOWN, 0)) ||
            !BindBack(g, t) || !MakeRenderSurfaces(g, t)) {
            gpuw::PaintTargetFree(ctx);
            return false;
        }
    }
    BeginFrameState(g, T(ctx));
    return true;
}

bool PaintTargetEnd(PaintCtx* ctx) {
    GpuTarget* t = T(ctx);
    if (!t) {
        return false;
    }
    Flush();
    Gpu* g = &gGpu;
    if (t->msaaTex && t->backTex) {
        g->ctx->ResolveSubresource(t->backTex, 0, t->msaaTex, 0,
                                   DXGI_FORMAT_B8G8R8A8_UNORM);
    }
    gLastStats = gB.stats;
    gB.target = nullptr;
    // A frame the scene found identical to the last one is not presented.
    // The multisampled surface it would have resolved still holds the frame
    // before it, which is what is already on screen.
    if (scene::SkipPresent()) {
        return true;
    }
    // Sync interval 0, the way the D2D path presents, so the comparison is
    // between the drawing and nothing else.
    HRESULT hr = t->swap->Present(0, 0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        gpuw::PaintTargetFree(ctx);
        return false;
    }
    return true;
}

bool PaintTargetBeginOffscreen(PaintCtx* ctx, int pxW, int pxH) {
    if (!ctx || !ctx->pa || !EnsureGpu(ctx->pa) || pxW <= 0 || pxH <= 0) {
        return false;
    }
    Gpu* g = &gGpu;
    gpuw::PaintTargetFree(ctx);
    auto* t = new GpuTarget();
    t->offscreen = true;
    t->pxW = pxW;
    t->pxH = pxH;
    // One sample: the pixels are read straight back, so there is nothing to
    // resolve them into.
    t->samples = 1;
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = (UINT)pxW;
    td.Height = (UINT)pxH;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET;
    if (FAILED(g->dev->CreateTexture2D(&td, nullptr, &t->offTex)) ||
        FAILED(g->dev
                   ->CreateRenderTargetView(t->offTex, nullptr, &t->offRtv))) {
        Rel(&t->offRtv);
        Rel(&t->offTex);
        delete t;
        return false;
    }
    td.Usage = D3D11_USAGE_STAGING;
    td.BindFlags = 0;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (FAILED(g->dev->CreateTexture2D(&td, nullptr, &t->stage))) {
        Rel(&t->offRtv);
        Rel(&t->offTex);
        delete t;
        return false;
    }
    if (!MakeRenderSurfaces(g, t)) {
        Rel(&t->stage);
        Rel(&t->offRtv);
        Rel(&t->offTex);
        delete t;
        return false;
    }
    ctx->rt = (PaintTarget*)t;
    BeginFrameState(g, t);
    float clear[4] = {0, 0, 0, 0};
    g->ctx->ClearRenderTargetView(t->offRtv, clear);
    return true;
}

bool PaintTargetEndOffscreen(PaintCtx* ctx, uint8_t* outBgra) {
    GpuTarget* t = T(ctx);
    if (!t || !t->offscreen) {
        return false;
    }
    Flush();
    Gpu* g = &gGpu;
    g->ctx->CopyResource(t->stage, t->offTex);
    if (outBgra) {
        D3D11_MAPPED_SUBRESOURCE m = {};
        if (SUCCEEDED(g->ctx->Map(t->stage, 0, D3D11_MAP_READ, 0, &m))) {
            for (int y = 0; y < t->pxH; y++) {
                memcpy(outBgra + (size_t)y * (size_t)t->pxW * 4,
                       (const uint8_t*)m.pData + (size_t)y * m.RowPitch,
                       (size_t)t->pxW * 4);
            }
            g->ctx->Unmap(t->stage, 0);
        }
    }
    gB.target = nullptr;
    gpuw::PaintTargetFree(ctx);
    return true;
}

// ─── canvas ──────────────────────────────────────────────────────────────

void CanvasClear(PaintCtx* ctx, Rgba c) {
    GpuTarget* t = T(ctx);
    if (!t) {
        return;
    }
    // A clear only reaches the whole surface when nothing is clipping it; a
    // clip in force makes this an ordinary filled rect, the way cairo's
    // paint-under-clip is.
    bool clipped = gB.clipStack.len > 0;
    if (!clipped) {
        Flush();
        Rgba f = PaintFade(ctx, c);
        float col[4] = {f.r / 255.f, f.g / 255.f, f.b / 255.f, f.a / 255.f};
        ID3D11RenderTargetView* rtv =
            t->offscreen ? t->offRtv : (t->msaaRtv ? t->msaaRtv : t->backRtv);
        gGpu.ctx->ClearRenderTargetView(rtv, col);
        return;
    }
    Quad(ctx, kQuadRect, 0, 0, (float)t->pxW, (float)t->pxH, c);
}

void CanvasFillRect(PaintCtx* ctx, float x, float y, float w, float h, Rgba c) {
    Quad(ctx, kQuadRect, x, y, w, h, c);
}

void CanvasFillRound(PaintCtx* ctx, float x, float y, float w, float h, float r,
                     Rgba c) {
    Quad(ctx, kQuadRect, x, y, w, h, c, r);
}

void CanvasStrokeRound(PaintCtx* ctx, float x, float y, float w, float h,
                       float r, float stroke, Rgba c, const float* dash) {
    // Dashes on a rounded rect would need the outline walked and cut, which
    // is more than a prototype needs: the tree's dashed rects are focus rings
    // and chart frames, and a solid one shows the same box in the same place.
    (void)dash;
    if (stroke <= 0 || w <= 0 || h <= 0) {
        return;
    }
    Quad(ctx, kQuadBorder, x, y, w, h, c, r, stroke);
}

// A segment as a quad, which is what the triangle pipeline is for: the quad
// pipeline only draws boxes that are square with the screen.
static void StrokeSegment(PaintCtx* ctx, float x1, float y1, float x2, float y2,
                          float wdt, Rgba c) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-6f) {
        return;
    }
    float nx = -dy / len * wdt * 0.5f;
    float ny = dx / len * wdt * 0.5f;
    (void)ctx;
    TriVertex(x1 + nx, y1 + ny, c);
    TriVertex(x2 + nx, y2 + ny, c);
    TriVertex(x2 - nx, y2 - ny, c);
    TriVertex(x1 + nx, y1 + ny, c);
    TriVertex(x2 - nx, y2 - ny, c);
    TriVertex(x1 - nx, y1 - ny, c);
}

// A disc, as the fan a round cap or a round join is. Twelve segments is past
// the point where another one shows at the widths this tree strokes at.
static void StrokeDisc(float cx, float cy, float r, Rgba c) {
    const int kSeg = 12;
    for (int i = 0; i < kSeg; i++) {
        float a0 = (float)i / kSeg * 2.f * kPi;
        float a1 = (float)(i + 1) / kSeg * 2.f * kPi;
        TriVertex(cx, cy, c);
        TriVertex(cx + cosf(a0) * r, cy + sinf(a0) * r, c);
        TriVertex(cx + cosf(a1) * r, cy + sinf(a1) * r, c);
    }
}

void CanvasLine(PaintCtx* ctx, float x1, float y1, float x2, float y2,
                float stroke, Rgba c, const float* dash) {
    if (!T(ctx) || stroke <= 0) {
        return;
    }
    c = PaintFade(ctx, c);
    if (c.a == 0) {
        return;
    }
    if (!dash) {
        StrokeSegment(ctx, x1, y1, x2, y2, stroke, c);
        return;
    }
    // The pattern is in stroke widths, the way D2D measures it. Cut the
    // segment up on the CPU, which is what a dash is.
    float on = dash[0] * stroke;
    float off = dash[1] * stroke;
    if (on <= 0 || off < 0) {
        StrokeSegment(ctx, x1, y1, x2, y2, stroke, c);
        return;
    }
    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-6f) {
        return;
    }
    dx /= len;
    dy /= len;
    for (float at = 0; at < len;) {
        float end = at + on;
        if (end > len) {
            end = len;
        }
        StrokeSegment(ctx, x1 + dx * at, y1 + dy * at, x1 + dx * end,
                      y1 + dy * end, stroke, c);
        at = end + off;
    }
}

void CanvasEllipse(PaintCtx* ctx, float cx, float cy, float rx, float ry,
                   float stroke, Rgba c) {
    if (rx <= 0 || ry <= 0) {
        return;
    }
    if (stroke > 0) {
        Quad(ctx, kQuadEllipseB, cx - rx, cy - ry, rx * 2, ry * 2, c, 0,
             stroke);
    } else {
        Quad(ctx, kQuadEllipse, cx - rx, cy - ry, rx * 2, ry * 2, c);
    }
}

void CanvasPushClip(PaintCtx* ctx, float x, float y, float w, float h) {
    if (!T(ctx)) {
        return;
    }
    // Nothing is flushed: the mask rides on the instance, so a clip change
    // costs four floats and never breaks a batch. That is the whole reason
    // GPUI carries a content mask per primitive.
    for (int i = 0; i < 4; i++) {
        gB.clipStack.Append(gB.clip[i]);
    }
    float x0 = x > gB.clip[0] ? x : gB.clip[0];
    float y0 = y > gB.clip[1] ? y : gB.clip[1];
    float x1 = (x + w) < gB.clip[2] ? (x + w) : gB.clip[2];
    float y1 = (y + h) < gB.clip[3] ? (y + h) : gB.clip[3];
    gB.clip[0] = x0;
    gB.clip[1] = y0;
    gB.clip[2] = x1 > x0 ? x1 : x0;
    gB.clip[3] = y1 > y0 ? y1 : y0;
}

void CanvasPopClip(PaintCtx* ctx) {
    (void)ctx;
    if (gB.clipStack.len < 4) {
        return;
    }
    for (int i = 0; i < 4; i++) {
        gB.clip[3 - i] = gB.clipStack[gB.clipStack.len - 1 - i];
    }
    gB.clipStack.len -= 4;
}

// ─── paths ───────────────────────────────────────────────────────────────

static GpuPath* P(Path* p) {
    return (GpuPath*)p;
}

Path* PathNew(PaintCtx* ctx, bool winding) {
    if (!ctx) {
        return nullptr;
    }
    auto* p = new GpuPath();
    p->winding = winding;
    return (Path*)p;
}

void PathFree(Path* path) {
    delete P(path);
}

static void PathPoint(GpuPath* p, float x, float y) {
    p->pts.Append(x);
    p->pts.Append(y);
    if (x < p->minX) {
        p->minX = x;
    }
    if (y < p->minY) {
        p->minY = y;
    }
    if (x > p->maxX) {
        p->maxX = x;
    }
    if (y > p->maxY) {
        p->maxY = y;
    }
}

static void MoveTo(GpuPath* p, float x, float y) {
    p->starts.Append(p->pts.len / 2);
    PathPoint(p, x, y);
    p->open = true;
}

void PathMoveTo(Path* path, float x, float y) {
    if (path) {
        MoveTo(P(path), x, y);
    }
}

void PathLineTo(Path* path, float x, float y) {
    GpuPath* p = P(path);
    if (!p) {
        return;
    }
    if (!p->open) {
        MoveTo(p, x, y);
        return;
    }
    PathPoint(p, x, y);
}

void PathCubicTo(Path* path, float x1, float y1, float x2, float y2, float x,
                 float y) {
    GpuPath* p = P(path);
    if (!p) {
        return;
    }
    if (!p->open) {
        MoveTo(p, x, y);
        return;
    }
    float x0 = p->pts[p->pts.len - 2];
    float y0 = p->pts[p->pts.len - 1];
    // A fixed subdivision rather than an adaptive one. The curves here are
    // icon outlines and chart shoulders a few tens of pixels across, where
    // sixteen segments is already under a pixel of error.
    const int kSeg = 16;
    for (int i = 1; i <= kSeg; i++) {
        float t = (float)i / kSeg;
        float u = 1.f - t;
        float bx = u * u * u * x0 + 3 * u * u * t * x1 + 3 * u * t * t * x2 +
                   t * t * t * x;
        float by = u * u * u * y0 + 3 * u * u * t * y1 + 3 * u * t * t * y2 +
                   t * t * t * y;
        PathPoint(p, bx, by);
    }
}

void PathArcTo(Path* path, float cx, float cy, float r, float a0, float a1,
               bool clockwise) {
    GpuPath* p = P(path);
    if (!p) {
        return;
    }
    float sweep = a1 - a0;
    if (clockwise && sweep < 0) {
        sweep += 2.f * kPi;
    }
    if (!clockwise && sweep > 0) {
        sweep -= 2.f * kPi;
    }
    // One segment per six degrees, and never fewer than two.
    int n = (int)(fabsf(sweep) / (kPi / 30.f)) + 2;
    if (n > 256) {
        n = 256;
    }
    for (int i = 0; i <= n; i++) {
        float a = a0 + sweep * ((float)i / (float)n);
        float x = cx + cosf(a) * r;
        float y = cy + sinf(a) * r;
        if (i == 0 && !p->open) {
            MoveTo(p, x, y);
        } else {
            PathPoint(p, x, y);
        }
    }
    p->open = true;
}

void PathClose(Path* path) {
    GpuPath* p = P(path);
    if (!p || !p->open) {
        return;
    }
    // A contour is closed by the fill rule, not by a repeated point: the fan
    // below always joins the last point back to the first.
    p->open = false;
}

static int ContourEnd(const GpuPath* p, int ci) {
    return ci + 1 < p->starts.len ? p->starts[ci + 1] : p->pts.len / 2;
}

// The stencil pass: for every contour, the fan (first, i, i+1). It may
// self-overlap and wind either way — that is what the stencil op is counting.
static void PathStencil(PaintCtx* ctx, GpuPath* p) {
    (void)ctx;
    Rgba none = {};
    for (int ci = 0; ci < p->starts.len; ci++) {
        int a = p->starts[ci];
        int b = ContourEnd(p, ci);
        if (b - a < 3) {
            continue;
        }
        float ax = p->pts[a * 2];
        float ay = p->pts[a * 2 + 1];
        for (int i = a + 1; i + 1 < b; i++) {
            TriVertex(ax, ay, none);
            TriVertex(p->pts[i * 2], p->pts[i * 2 + 1], none);
            TriVertex(p->pts[(i + 1) * 2], p->pts[(i + 1) * 2 + 1], none);
        }
    }
}

static void PathCover(PaintCtx* ctx, GpuPath* p, const Inst& proto) {
    Gpu* g = &gGpu;
    // The bounding box, one pixel out so an antialiased edge is not clipped
    // by its own cover quad.
    // PathFillWith flushed before the stencil pass, so this is the only
    // instance in the buffer and SV_InstanceID will read it at 0 — D3D11 does
    // not fold StartInstanceLocation into that id.
    Inst i = proto;
    i.rect[0] = p->minX - 1;
    i.rect[1] = p->minY - 1;
    i.rect[2] = (p->maxX - p->minX) + 2;
    i.rect[3] = (p->maxY - p->minY) + 2;
    memcpy(i.clip, gB.clip, sizeof(i.clip));
    gB.insts.len = 0;
    gB.insts.Append(i);
    (void)ctx;

    // Straight out rather than through FlushQuads: this one draw needs the
    // stencil test, and the state goes back afterwards.
    if (!EnsureInstBuf(g, gB.insts.len)) {
        gB.insts.len = 0;
        return;
    }
    D3D11_MAPPED_SUBRESOURCE m = {};
    if (FAILED(g->ctx->Map(g->instBuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
        gB.insts.len = 0;
        return;
    }
    memcpy(m.pData, gB.insts.els, (size_t)gB.insts.len * sizeof(Inst));
    g->ctx->Unmap(g->instBuf, 0);
    ID3D11ShaderResourceView* srvs[3] = {g->instSrv, g->atlas.srv, g->white};
    g->ctx->VSSetShader(g->vsQuad, nullptr, 0);
    g->ctx->PSSetShader(g->psQuad, nullptr, 0);
    g->ctx->VSSetShaderResources(0, 3, srvs);
    g->ctx->PSSetShaderResources(0, 3, srvs);
    g->ctx->IASetInputLayout(nullptr);
    g->ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    g->ctx->OMSetDepthStencilState(g->dsCover, 0);
    g->ctx->DrawInstanced(4, 1, 0, 0);
    g->ctx->OMSetDepthStencilState(g->dsOff, 0);
    gB.stats.instances++;
    gB.stats.draws++;
    gB.insts.len = 0;
}

static void PathFillWith(PaintCtx* ctx, Path* path, const Inst& proto) {
    GpuPath* p = P(path);
    if (!T(ctx) || !p || p->pts.len < 6) {
        return;
    }
    Flush();
    PathStencil(ctx, p);
    FlushTris(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false,
              p->winding ? gGpu.dsNonZero : gGpu.dsEvenOdd);
    PathCover(ctx, p, proto);
}

// Nothing to do: a GpuPath is already the flattened contours the stencil
// pass draws, and it was built once. What the D2D backend gains here the GPU
// one gets by construction.
void PathRealize(PaintCtx* ctx, Path* p) {
    (void)ctx;
    (void)p;
}

void PathFill(PaintCtx* ctx, Path* p, Rgba c) {
    c = PaintFade(ctx, c);
    if (c.a == 0) {
        return;
    }
    Inst proto = {};
    SetColor(proto.color, c);
    proto.misc[2] = (float)kQuadSolid;
    PathFillWith(ctx, p, proto);
}

void PathFillGradient(PaintCtx* ctx, Path* p, float x0, float y0, float x1,
                      float y1, Rgba from, Rgba to) {
    from = PaintFade(ctx, from);
    to = PaintFade(ctx, to);
    Inst proto = {};
    SetColor(proto.color, from);
    SetColor(proto.color2, to);
    proto.misc[2] = (float)kQuadGradient;
    proto.uv[0] = x0;
    proto.uv[1] = y0;
    proto.uv[2] = x1;
    proto.uv[3] = y1;
    PathFillWith(ctx, p, proto);
}

void PathStroke(PaintCtx* ctx, Path* path, float stroke, Rgba c,
                bool roundCaps) {
    GpuPath* p = P(path);
    if (!T(ctx) || !p || stroke <= 0) {
        return;
    }
    c = PaintFade(ctx, c);
    if (c.a == 0) {
        return;
    }
    for (int ci = 0; ci < p->starts.len; ci++) {
        int a = p->starts[ci];
        int b = ContourEnd(p, ci);
        for (int i = a; i + 1 < b; i++) {
            StrokeSegment(ctx, p->pts[i * 2], p->pts[i * 2 + 1],
                          p->pts[(i + 1) * 2], p->pts[(i + 1) * 2 + 1], stroke,
                          c);
        }
        // A disc at every joint, which is a round join. The tree strokes
        // chart lines and icon outlines, where a miter and a round join are a
        // fraction of a pixel apart at these widths.
        if (stroke > 1.5f) {
            int first = roundCaps ? a : a + 1;
            int last = roundCaps ? b : b - 1;
            for (int i = first; i < last; i++) {
                StrokeDisc(p->pts[i * 2], p->pts[i * 2 + 1], stroke * 0.5f, c);
            }
        }
    }
}

// ─── images ──────────────────────────────────────────────────────────────
//
// The WIC decode is shared with the D2D backend; what is not shared is the
// texture, which is made once per image and hung off a small table rather
// than off the Image, whose layout belongs to the other backend.

constexpr int kImageSlots = 32;

struct ImageSlot {
    const Image* img = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
};

static ImageSlot gImages[kImageSlots];
static int gImageNext = 0;

static ID3D11ShaderResourceView* ImageSrv(const Image* img) {
    for (int i = 0; i < kImageSlots; i++) {
        if (gImages[i].img == img) {
            return gImages[i].srv;
        }
    }
    const uint8_t* bgra = nullptr;
    int w = 0, h = 0;
    if (!PaintImagePixels(img, &bgra, &w, &h) || !bgra || w <= 0 || h <= 0) {
        return nullptr;
    }
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = (UINT)w;
    td.Height = (UINT)h;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA sd = {};
    sd.pSysMem = bgra;
    sd.SysMemPitch = (UINT)(w * 4);
    ID3D11Texture2D* tex = nullptr;
    if (FAILED(gGpu.dev->CreateTexture2D(&td, &sd, &tex))) {
        return nullptr;
    }
    ID3D11ShaderResourceView* srv = nullptr;
    HRESULT hr = gGpu.dev->CreateShaderResourceView(tex, nullptr, &srv);
    tex->Release();
    if (FAILED(hr)) {
        return nullptr;
    }
    ImageSlot* s = &gImages[gImageNext];
    gImageNext = (gImageNext + 1) % kImageSlots;
    Rel(&s->srv);
    s->img = img;
    s->srv = srv;
    return srv;
}

void ImageDraw(PaintCtx* ctx, Image* img, Bounds b) {
    if (!T(ctx) || !img || b.w <= 0 || b.h <= 0) {
        return;
    }
    ID3D11ShaderResourceView* srv = ImageSrv(img);
    if (!srv) {
        return;
    }
    EnsureQuadPhase();
    // A different texture is the one thing that has to break the batch.
    if (gB.image != srv) {
        FlushQuads();
        gB.image = srv;
    }
    Inst i = {};
    i.rect[0] = b.x;
    i.rect[1] = b.y;
    i.rect[2] = b.w;
    i.rect[3] = b.h;
    float op = ctx->opacity < 0 ? 0 : (ctx->opacity > 1 ? 1 : ctx->opacity);
    i.color[3] = op;
    i.misc[2] = (float)kQuadImage;
    i.uv[2] = 1.f;
    i.uv[3] = 1.f;
    memcpy(i.clip, gB.clip, sizeof(i.clip));
    Push(i);
    FlushQuads();
    gB.image = nullptr;
}

// ─── text ────────────────────────────────────────────────────────────────
//
// The layout is an IDWriteTextLayout, exactly as it is on the D2D path — so
// shaping, measurement, hit-testing and the range rects are literally the
// same code, and only the drawing is different. IDWriteTextLayout::Draw hands
// its glyph runs to the renderer below, which turns each glyph into a quad
// against the atlas.

static GlyphEntry* AtlasFind(const GlyphKey& k) {
    uint32_t at = GlyphHash(k);
    for (int probe = 0; probe < 32; probe++) {
        GlyphEntry* e = &gGpu.atlas.slots[(at + probe) & (kGlyphSlots - 1)];
        if (!e->used) {
            return e; // free slot, and therefore not present
        }
        if (e->key.face == k.face && e->key.glyph == k.glyph &&
            e->key.size4 == k.size4) {
            return e;
        }
    }
    return nullptr;
}

// Rasterize one glyph at the origin and shelf it. DirectWrite only hands out
// a ClearType 3x1 texture for an antialiased rendering mode, so the three
// subpixel channels are averaged back into the one coverage value the shader
// wants — which is what asking for grayscale antialiasing would have given.
static bool AtlasRasterize(IDWriteFontFace* face, float em, uint16_t glyph,
                           GlyphEntry* e) {
    Gpu* g = &gGpu;
    float advance = 0;
    DWRITE_GLYPH_OFFSET offset = {};
    DWRITE_GLYPH_RUN run = {};
    run.fontFace = face;
    run.fontEmSize = em;
    run.glyphCount = 1;
    run.glyphIndices = &glyph;
    run.glyphAdvances = &advance;
    run.glyphOffsets = &offset;

    IDWriteGlyphRunAnalysis* an = nullptr;
    HRESULT hr = g->dwrite->CreateGlyphRunAnalysis(
        &run, 1.f, nullptr, DWRITE_RENDERING_MODE_NATURAL,
        DWRITE_MEASURING_MODE_NATURAL, 0.f, 0.f, &an);
    if (FAILED(hr) || !an) {
        return false;
    }
    RECT r = {};
    hr = an->GetAlphaTextureBounds(DWRITE_TEXTURE_CLEARTYPE_3x1, &r);
    int w = r.right - r.left;
    int h = r.bottom - r.top;
    if (FAILED(hr) || w <= 0 || h <= 0 || w > 512 || h > 512) {
        an->Release();
        // A blank glyph — a space — is a real answer: nothing to draw, and
        // the entry stops it being asked for again.
        e->used = true;
        e->w = 0;
        e->h = 0;
        return SUCCEEDED(hr);
    }
    // Static scratch: a glyph is rasterized once and this is the only place
    // that touches these, so the buffers are reused rather than grown per
    // glyph.
    static Vec<uint8_t> rgb;
    static Vec<uint8_t> gray;
    rgb.len = 0;
    if (!rgb.AppendBlanks(w * h * 3)) {
        an->Release();
        return false;
    }
    hr = an->CreateAlphaTexture(DWRITE_TEXTURE_CLEARTYPE_3x1, &r, rgb.els,
                                (UINT32)(w * h * 3));
    an->Release();
    if (FAILED(hr)) {
        return false;
    }
    gray.len = 0;
    if (!gray.AppendBlanks(w * h)) {
        return false;
    }
    for (int i = 0; i < w * h; i++) {
        int s = (int)rgb[i * 3] + rgb[i * 3 + 1] + rgb[i * 3 + 2];
        gray[i] = (uint8_t)((s + 1) / 3);
    }

    Atlas* a = &g->atlas;
    if (a->penX + w + 1 > kAtlasDim) {
        a->penX = 0;
        a->penY += a->rowH + 1;
        a->rowH = 0;
    }
    if (a->penY + h + 1 > kAtlasDim) {
        // Full. Start over rather than grow: a UI's glyph set is small, and
        // this only ever happens on a document that changed size a lot.
        memset(a->slots, 0, sizeof(a->slots));
        a->penX = 0;
        a->penY = 0;
        a->rowH = 0;
        e = AtlasFind({face, glyph, (uint32_t)(em * 4.f + 0.5f)});
        if (!e) {
            return false;
        }
    }
    D3D11_BOX box = {};
    box.left = (UINT)a->penX;
    box.top = (UINT)a->penY;
    box.right = (UINT)(a->penX + w);
    box.bottom = (UINT)(a->penY + h);
    box.back = 1;
    g->ctx->UpdateSubresource(a->tex, 0, &box, gray.els, (UINT)w, 0);

    e->used = true;
    e->key.face = face;
    e->key.glyph = glyph;
    e->key.size4 = (uint32_t)(em * 4.f + 0.5f);
    e->x = a->penX;
    e->y = a->penY;
    e->w = w;
    e->h = h;
    e->bearingX = r.left;
    e->bearingY = r.top;
    a->penX += w + 1;
    if (h > a->rowH) {
        a->rowH = h;
    }
    gB.stats.glyphsRasterized++;
    return true;
}

static void DrawGlyph(IDWriteFontFace* face, float em, uint16_t glyph, float x,
                      float y, Rgba c) {
    GlyphKey k = {face, glyph, (uint32_t)(em * 4.f + 0.5f)};
    GlyphEntry* e = AtlasFind(k);
    if (!e) {
        return;
    }
    if (!e->used && !AtlasRasterize(face, em, glyph, e)) {
        return;
    }
    if (e->w <= 0 || e->h <= 0) {
        return;
    }
    // Snapped to whole pixels. DirectWrite would have positioned this at a
    // third of one; the atlas holds a single rasterization per glyph, so this
    // is the prototype's one visible difference from the D2D path.
    float gx = floorf(x + 0.5f) + (float)e->bearingX;
    float gy = floorf(y + 0.5f) + (float)e->bearingY;

    Inst i = {};
    i.rect[0] = gx;
    i.rect[1] = gy;
    i.rect[2] = (float)e->w;
    i.rect[3] = (float)e->h;
    SetColor(i.color, c);
    i.misc[2] = (float)kQuadGlyph;
    i.uv[0] = (float)e->x / kAtlasDim;
    i.uv[1] = (float)e->y / kAtlasDim;
    i.uv[2] = (float)(e->x + e->w) / kAtlasDim;
    i.uv[3] = (float)(e->y + e->h) / kAtlasDim;
    memcpy(i.clip, gB.clip, sizeof(i.clip));
    Push(i);
}

// The minimum IDWriteTextRenderer that a layout will talk to. No refcount
// worth the name: one of these lives on the stack for the length of a Draw.
struct GlyphSink : public IDWriteTextRenderer {
    Rgba color = {};
    PaintCtx* ctx = nullptr;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** obj) override {
        if (iid == __uuidof(IUnknown) ||
            iid == __uuidof(IDWritePixelSnapping) ||
            iid == __uuidof(IDWriteTextRenderer)) {
            *obj = this;
            return S_OK;
        }
        *obj = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }

    HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*,
                                                      BOOL* out) override {
        *out = FALSE;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*,
                                                  DWRITE_MATRIX* m) override {
        DWRITE_MATRIX id = {1, 0, 0, 1, 0, 0};
        *m = id;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*, FLOAT* out) override {
        *out = 1.f;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*, FLOAT baselineX,
                                           FLOAT baselineY,
                                           DWRITE_MEASURING_MODE,
                                           const DWRITE_GLYPH_RUN* run,
                                           const DWRITE_GLYPH_RUN_DESCRIPTION*,
                                           IUnknown*) override {
        if (!run || !run->fontFace || run->glyphCount == 0) {
            return S_OK;
        }
        bool rtl = (run->bidiLevel & 1) != 0;
        float x = baselineX;
        for (UINT32 i = 0; i < run->glyphCount; i++) {
            float adv = run->glyphAdvances ? run->glyphAdvances[i] : 0.f;
            float ox = 0, oy = 0;
            if (run->glyphOffsets) {
                ox = run->glyphOffsets[i].advanceOffset;
                oy = run->glyphOffsets[i].ascenderOffset;
            }
            if (rtl) {
                x -= adv;
            }
            DrawGlyph(run->fontFace, run->fontEmSize, run->glyphIndices[i],
                      x + (rtl ? -ox : ox), baselineY - oy, color);
            if (!rtl) {
                x += adv;
            }
        }
        return S_OK;
    }

    // An underline or a strikethrough is a rule under a pixel tall — 0.8 at
    // the sizes here — and left to the SDF it comes out at the coverage of a
    // 0.8 px band, which is visibly fainter than the same rule on the D2D
    // path. DirectWrite snaps its own rules to the pixel grid; so does this.
    void Rule(float x, float y, float w, float thickness) {
        float t = thickness > 1.f ? thickness : 1.f;
        Quad(ctx, kQuadRect, x, floorf(y + 0.5f), w, t, color);
    }

    HRESULT STDMETHODCALLTYPE DrawUnderline(void*, FLOAT baselineX,
                                            FLOAT baselineY,
                                            const DWRITE_UNDERLINE* u,
                                            IUnknown*) override {
        if (u) {
            Rule(baselineX, baselineY + u->offset, u->width, u->thickness);
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*, FLOAT baselineX,
                                                FLOAT baselineY,
                                                const DWRITE_STRIKETHROUGH* s,
                                                IUnknown*) override {
        if (s) {
            Rule(baselineX, baselineY + s->offset, s->width, s->thickness);
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawInlineObject(void*, FLOAT, FLOAT,
                                               IDWriteInlineObject*, BOOL, BOOL,
                                               IUnknown*) override {
        return E_NOTIMPL;
    }
};

void TextLayoutDraw(PaintCtx* ctx, TextLayout* tl, float x, float y, Rgba c,
                    bool clip) {
    if (!T(ctx) || !tl) {
        return;
    }
    auto* layout = (IDWriteTextLayout*)tl;
    Rgba faded = PaintFade(ctx, c);
    if (faded.a == 0) {
        return;
    }
    bool pushed = false;
    if (clip) {
        DWRITE_TEXT_METRICS m = {};
        if (SUCCEEDED(layout->GetMetrics(&m))) {
            float w = layout->GetMaxWidth();
            gpuw::CanvasPushClip(ctx, x, y, w > 0 ? w : m.width, m.height);
            pushed = true;
        }
    }
    GlyphSink sink;
    sink.color = faded;
    sink.ctx = ctx;
    layout->Draw(nullptr, &sink, x, y);
    if (pushed) {
        gpuw::CanvasPopClip(ctx);
    }
}

} // namespace gpuw
} // namespace gpui

#endif // GPUI_OS_WINDOWS
