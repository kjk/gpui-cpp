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
