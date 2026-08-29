/* The scene: recording, ordering, hashing, diffing and replay. Portable —
   nothing here names an OS or a GPU type, because the replay draws by calling
   the same Paint.h entry points the element tree would have called, and a
   backend cannot tell the difference. See scene.h for what it is for and what
   it is short of.

   Only the Windows entry points dispatch into the recorder today; hooking a
   second platform up is the same one line at the top of each Paint.h entry
   point that paint_win.cpp has. */

#include "gpui/scene.h"

#include <string.h>

namespace gpui {

int SceneLevelOn() {
#if GPUI_OS_WINDOWS
    static_assert((int)WinSceneMode::Off == kSceneOff);
    static_assert((int)WinSceneMode::Replay == kSceneReplay);
    static_assert((int)WinSceneMode::Cache == kSceneCache);
    static_assert((int)WinSceneMode::Skip == kSceneSkip);
    static_assert((int)WinSceneMode::Damage == kSceneDamage);
    return (int)WinPaintOptionsGet().scene;
#else
    // Only the Windows paint front end records a scene today.
    return kSceneSkip;
#endif
}

namespace scene {

// ─── the recorded frame ──────────────────────────────────────────────────

enum PrimKind : uint8_t {
    kPClear = 0,
    kPRect,
    kPRound,
    kPStrokeRound,
    kPLine,
    kPEllipse,
    kPPathFill,
    kPPathGradient,
    kPPathStroke,
    kPImage,
    kPText
};

enum PrimFlag : uint8_t {
    kFDash = 1,      // e2 / e3 hold the dash pattern
    kFRoundCaps = 2, // PathStroke's round caps
    kFClip = 4       // TextLayoutDraw's clip-to-layout-box
};

// One thing to draw. GPUI's primitives are a struct per kind in a vector per
// kind; one POD struct for all of them costs a few floats a primitive and
// keeps the frame in one array, which is what the hash and the diff want.
//
// `mask` is the content mask, already intersected down the clip stack, so a
// primitive says where it may draw without reference to any stack. That is
// the property that makes the list reorderable and the replay's clip changes
// rare.
struct Prim {
    uint8_t kind = 0;
    uint8_t layer = 0;
    uint8_t flags = 0;
    uint8_t pad = 0;
    uint32_t seq = 0;
    // Geometry: a rect's x/y/w/h, a line's x1/y1/x2/y2, an ellipse's
    // cx/cy/rx/ry, a text run's x/y.
    float g0 = 0, g1 = 0, g2 = 0, g3 = 0;
    // Extras: radius, stroke width, and either the dash pattern or a
    // gradient's second point.
    float e0 = 0, e1 = 0, e2 = 0, e3 = 0;
    Bounds mask = {};
    // What this primitive covers, for the damage rectangle. Already clipped
    // to the mask.
    Bounds bbox = {};
    Rgba color = {};
    Rgba color2 = {};
    int32_t path = -1;
    // Image* or TextLayout*, both of which outlive the frame in a cache above
    // this layer.
    void* ref = nullptr;
    uint64_t hash = 0;
};

enum PathVerb : uint8_t {
    kVMove = 0, // 2 floats
    kVLine,     // 2
    kVCubic,    // 6
    kVArc,      // 5 + a direction in the low bit of the verb's high nibble
    kVClose     // 0
};

// A path as verbs and points rather than as a backend object, which is what
// lets it be hashed and so cached. The arc direction rides in `dirs` because
// a verb byte is a kind and nothing else.
struct PathRec {
    int verbFirst = 0, verbCount = 0;
    int ptFirst = 0, ptCount = 0;
    bool winding = false;
    bool hashed = false;
    uint64_t hash = 0;
    // Grown as points arrive; empty until the first one.
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    bool any = false;
};

static Vec<Prim> gCur;
static Vec<Prim> gPrev;
static Vec<PathRec> gPaths;
static Vec<uint8_t> gVerbs;
static Vec<float> gPts;

// The clip stack while recording, four floats a level, mirroring what the GPU
// backend does with its own: a push intersects, a pop restores.
static Vec<float> gClipStack;
static Bounds gClip = {};
static bool gRecording = false;
static uint32_t gSeq = 0;
static float gViewW = 0, gViewH = 0;
static bool gSkipPresent = false;
static SceneStats gStats;
static uint64_t gPrevFrameHash = 0;
static bool gHavePrev = false;
static int gFrameNo = 0;

// The damage of the last frames that were actually presented. The swap chain
// is FLIP_SEQUENTIAL with three buffers, so the buffer a frame is handed back
// holds what was drawn three presents ago: a partial redraw has to cover
// everything that has changed since then, not only what changed this frame.
static const int kBufferDepth = 3;
static Bounds gDamageRing[kBufferDepth] = {};
static int gDamageRingAt = 0;

bool Recording() {
    return gRecording;
}
bool SuspendBegin() {
    bool prev = gRecording;
    gRecording = false;
    return prev;
}
void SuspendEnd(bool prev) {
    gRecording = prev;
}
bool SkipPresent() {
    return gSkipPresent;
}
const SceneStats& Stats() {
    return gStats;
}

// ─── hashing ─────────────────────────────────────────────────────────────

static inline uint64_t HashBytes(uint64_t h, const void* p, int n) {
    const uint8_t* b = (const uint8_t*)p;
    for (int i = 0; i < n; i++) {
        h ^= b[i];
        h *= 0x100000001b3ull;
    }
    return h;
}
static const uint64_t kHashSeed = 0xcbf29ce484222325ull;

// Everything about a primitive that decides what appears on screen. `seq` and
// `bbox` are left out: the first is where it sat in the list, which the
// position in the list already says, and the second is derived.
// Two floats as one word, so the hash below runs eight bytes at a time.
static inline uint64_t Pair(float a, float b) {
    uint32_t x = 0, y = 0;
    memcpy(&x, &a, 4);
    memcpy(&y, &b, 4);
    return ((uint64_t)y << 32) | x;
}

// Everything about a primitive that decides what appears on screen. `seq` and
// `bbox` are left out: the first is where it sat in the list, which the
// position in the list already says, and the second is derived.
//
// Ten words rather than the hundred-odd bytes they occupy, because this runs
// over every primitive of every frame, and a byte-at-a-time FNV over a whole
// scene is a measurable share of what the scene costs — most of it on a frame
// where nothing can be cached, which is the one case where all of this is a
// loss rather than a win.
static uint64_t HashPrim(const Prim& p) {
    uint64_t w[10];
    w[0] =
        (uint64_t)p.kind | ((uint64_t)p.layer << 8) | ((uint64_t)p.flags << 16);
    w[1] = Pair(p.g0, p.g1);
    w[2] = Pair(p.g2, p.g3);
    w[3] = Pair(p.e0, p.e1);
    w[4] = Pair(p.e2, p.e3);
    w[5] = Pair(p.mask.x, p.mask.y);
    w[6] = Pair(p.mask.w, p.mask.h);
    uint32_t c0 = 0, c1 = 0;
    memcpy(&c0, &p.color, 4);
    memcpy(&c1, &p.color2, 4);
    w[7] = ((uint64_t)c1 << 32) | c0;
    // A cached shaped run keeps its address across frames, so the pointer is
    // the run's identity. A run that was dropped and whose address was reused
    // by another would compare equal — but the run's own size is in g2/g3
    // above and its position and colour beside them, so the collision needs
    // text that shapes to the same size, in the same place, in the same
    // colour. See scene.h.
    w[8] = (uint64_t)(uintptr_t)p.ref;
    w[9] = (p.path >= 0 && p.path < gPaths.len) ? gPaths[p.path].hash : 0;
    uint64_t h = kHashSeed;
    for (int i = 0; i < 10; i++) {
        h ^= w[i];
        h *= 0x100000001b3ull;
        h ^= h >> 29;
    }
    return h;
}

static uint64_t HashPath(PathRec& pr) {
    if (pr.hashed) {
        return pr.hash;
    }
    uint64_t h = kHashSeed;
    uint8_t w = pr.winding ? 1 : 0;
    h = HashBytes(h, &w, 1);
    if (pr.verbCount > 0) {
        h = HashBytes(h, &gVerbs[pr.verbFirst], pr.verbCount);
    }
    if (pr.ptCount > 0) {
        h = HashBytes(h, &gPts[pr.ptFirst], pr.ptCount * (int)sizeof(float));
    }
    pr.hash = h;
    pr.hashed = true;
    return h;
}

// ─── bounds helpers ──────────────────────────────────────────────────────

static Bounds Intersect(Bounds a, Bounds b) {
    float x0 = a.x > b.x ? a.x : b.x;
    float y0 = a.y > b.y ? a.y : b.y;
    float x1 = a.Right() < b.Right() ? a.Right() : b.Right();
    float y1 = a.Bottom() < b.Bottom() ? a.Bottom() : b.Bottom();
    Bounds r = {x0, y0, x1 > x0 ? x1 - x0 : 0, y1 > y0 ? y1 - y0 : 0};
    return r;
}

static Bounds Union(Bounds a, Bounds b) {
    if (a.w <= 0 || a.h <= 0) {
        return b;
    }
    if (b.w <= 0 || b.h <= 0) {
        return a;
    }
    float x0 = a.x < b.x ? a.x : b.x;
    float y0 = a.y < b.y ? a.y : b.y;
    float x1 = a.Right() > b.Right() ? a.Right() : b.Right();
    float y1 = a.Bottom() > b.Bottom() ? a.Bottom() : b.Bottom();
    Bounds r = {x0, y0, x1 - x0, y1 - y0};
    return r;
}

static bool SameBounds(Bounds a, Bounds b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

// ─── recording ───────────────────────────────────────────────────────────

static Prim* Emit(PaintCtx* ctx, uint8_t kind, Bounds bbox) {
    Prim p;
    p.kind = kind;
    p.layer = ctx ? (uint8_t)ctx->paintLayer : 0;
    p.seq = gSeq++;
    p.mask = gClip;
    p.bbox = Intersect(bbox, gClip);
    VecAppend(gCur, p);
    return &gCur[gCur.len - 1];
}

void FrameBegin(PaintCtx* ctx) {
    gRecording = true;
    gSkipPresent = false;
    VecClear(gCur);
    VecClear(gPaths);
    VecClear(gVerbs);
    VecClear(gPts);
    VecClear(gClipStack);
    gSeq = 0;
    gViewW = ctx ? ctx->viewW : 0;
    gViewH = ctx ? ctx->viewH : 0;
    // The mask everything starts inside: the view. A primitive whose mask is
    // this one needs no clip at replay.
    gClip = Bounds{0, 0, gViewW, gViewH};
    gStats.clipPushes = 0;
    gStats.culled = 0;
}

void RecClear(PaintCtx* ctx, Rgba c) {
    Prim* p = Emit(ctx, kPClear, Bounds{0, 0, gViewW, gViewH});
    p->color = PaintFade(ctx, c);
}

void RecFillRect(PaintCtx* ctx, float x, float y, float w, float h, Rgba c) {
    Prim* p = Emit(ctx, kPRect, Bounds{x, y, w, h});
    p->g0 = x;
    p->g1 = y;
    p->g2 = w;
    p->g3 = h;
    p->color = PaintFade(ctx, c);
}

void RecFillRound(PaintCtx* ctx, float x, float y, float w, float h, float r,
                  Rgba c) {
    Prim* p = Emit(ctx, kPRound, Bounds{x, y, w, h});
    p->g0 = x;
    p->g1 = y;
    p->g2 = w;
    p->g3 = h;
    p->e0 = r;
    p->color = PaintFade(ctx, c);
}

void RecStrokeRound(PaintCtx* ctx, float x, float y, float w, float h, float r,
                    float stroke, Rgba c, const float* dash) {
    float s = stroke > 0 ? stroke : 0;
    Prim* p =
        Emit(ctx, kPStrokeRound, Bounds{x - s, y - s, w + s * 2, h + s * 2});
    p->g0 = x;
    p->g1 = y;
    p->g2 = w;
    p->g3 = h;
    p->e0 = r;
    p->e1 = stroke;
    if (dash) {
        p->flags |= kFDash;
        p->e2 = dash[0];
        p->e3 = dash[1];
    }
    p->color = PaintFade(ctx, c);
}

void RecLine(PaintCtx* ctx, float x1, float y1, float x2, float y2,
             float stroke, Rgba c, const float* dash) {
    float s = stroke > 0 ? stroke : 1;
    float lo = x1 < x2 ? x1 : x2, hi = x1 > x2 ? x1 : x2;
    float lo2 = y1 < y2 ? y1 : y2, hi2 = y1 > y2 ? y1 : y2;
    Prim* p = Emit(ctx, kPLine,
                   Bounds{lo - s, lo2 - s, hi - lo + s * 2, hi2 - lo2 + s * 2});
    p->g0 = x1;
    p->g1 = y1;
    p->g2 = x2;
    p->g3 = y2;
    p->e1 = stroke;
    if (dash) {
        p->flags |= kFDash;
        p->e2 = dash[0];
        p->e3 = dash[1];
    }
    p->color = PaintFade(ctx, c);
}

void RecEllipse(PaintCtx* ctx, float cx, float cy, float rx, float ry,
                float stroke, Rgba c) {
    float s = stroke > 0 ? stroke : 0;
    Prim* p =
        Emit(ctx, kPEllipse,
             Bounds{cx - rx - s, cy - ry - s, (rx + s) * 2, (ry + s) * 2});
    p->g0 = cx;
    p->g1 = cy;
    p->g2 = rx;
    p->g3 = ry;
    p->e1 = stroke;
    p->color = PaintFade(ctx, c);
}

void RecPushClip(PaintCtx* ctx, float x, float y, float w, float h) {
    (void)ctx;
    gStats.clipPushes++;
    VecAppend(gClipStack, gClip.x);
    VecAppend(gClipStack, gClip.y);
    VecAppend(gClipStack, gClip.w);
    VecAppend(gClipStack, gClip.h);
    gClip = Intersect(gClip, Bounds{x, y, w, h});
}

void RecPopClip(PaintCtx* ctx) {
    (void)ctx;
    if (gClipStack.len < 4) {
        return;
    }
    int n = gClipStack.len;
    gClip = Bounds{gClipStack[n - 4], gClipStack[n - 3], gClipStack[n - 2],
                   gClipStack[n - 1]};
    gClipStack.len -= 4;
}

// ─── recording: paths ────────────────────────────────────────────────────
//
// A recorded Path* is an index, not a pointer: it never reaches a backend,
// because every Paint.h path entry point asks Recording() before doing
// anything. One is added so that index 0 is not a null path.

static Path* PathHandle(int idx) {
    return (Path*)(uintptr_t)(idx + 1);
}
static PathRec* PathOf(Path* p) {
    int idx = (int)(uintptr_t)p - 1;
    if (idx < 0 || idx >= gPaths.len) {
        return nullptr;
    }
    return &gPaths[idx];
}

Path* RecPathNew(bool winding) {
    PathRec pr;
    pr.winding = winding;
    pr.verbFirst = gVerbs.len;
    pr.ptFirst = gPts.len;
    VecAppend(gPaths, pr);
    return PathHandle(gPaths.len - 1);
}

void RecPathFree(Path* p) {
    // The geometry has to outlive the call: the primitive that referred to it
    // is still in the list and the replay has not run. It goes with the
    // frame instead.
    (void)p;
}

static void Verb(PathRec* pr, uint8_t v) {
    VecAppend(gVerbs, v);
    pr->verbCount++;
    pr->hashed = false;
}

static void Pt(PathRec* pr, float x, float y) {
    VecAppend(gPts, x);
    VecAppend(gPts, y);
    pr->ptCount += 2;
    if (!pr->any) {
        pr->any = true;
        pr->x0 = pr->x1 = x;
        pr->y0 = pr->y1 = y;
    }
    pr->x0 = x < pr->x0 ? x : pr->x0;
    pr->y0 = y < pr->y0 ? y : pr->y0;
    pr->x1 = x > pr->x1 ? x : pr->x1;
    pr->y1 = y > pr->y1 ? y : pr->y1;
}

void RecPathMoveTo(Path* p, float x, float y) {
    PathRec* pr = PathOf(p);
    if (!pr) {
        return;
    }
    Verb(pr, kVMove);
    Pt(pr, x, y);
}

void RecPathLineTo(Path* p, float x, float y) {
    PathRec* pr = PathOf(p);
    if (!pr) {
        return;
    }
    Verb(pr, kVLine);
    Pt(pr, x, y);
}

void RecPathCubicTo(Path* p, float x1, float y1, float x2, float y2, float x,
                    float y) {
    PathRec* pr = PathOf(p);
    if (!pr) {
        return;
    }
    Verb(pr, kVCubic);
    Pt(pr, x1, y1);
    Pt(pr, x2, y2);
    Pt(pr, x, y);
}

void RecPathArcTo(Path* p, float cx, float cy, float r, float a0, float a1,
                  bool clockwise) {
    PathRec* pr = PathOf(p);
    if (!pr) {
        return;
    }
    Verb(pr, (uint8_t)(kVArc | (clockwise ? 0x80 : 0)));
    // Five values, and the bounding box of the whole circle: an arc's extent
    // is not its endpoints, and the damage rectangle may only be too big.
    VecAppend(gPts, cx);
    VecAppend(gPts, cy);
    VecAppend(gPts, r);
    VecAppend(gPts, a0);
    VecAppend(gPts, a1);
    pr->ptCount += 5;
    pr->hashed = false;
    if (!pr->any) {
        pr->any = true;
        pr->x0 = cx - r;
        pr->y0 = cy - r;
        pr->x1 = cx + r;
        pr->y1 = cy + r;
    } else {
        pr->x0 = (cx - r) < pr->x0 ? cx - r : pr->x0;
        pr->y0 = (cy - r) < pr->y0 ? cy - r : pr->y0;
        pr->x1 = (cx + r) > pr->x1 ? cx + r : pr->x1;
        pr->y1 = (cy + r) > pr->y1 ? cy + r : pr->y1;
    }
}

void RecPathClose(Path* p) {
    PathRec* pr = PathOf(p);
    if (!pr) {
        return;
    }
    Verb(pr, kVClose);
}

static Bounds PathBox(const PathRec* pr, float grow) {
    if (!pr || !pr->any) {
        return Bounds{0, 0, 0, 0};
    }
    return Bounds{pr->x0 - grow, pr->y0 - grow, pr->x1 - pr->x0 + grow * 2,
                  pr->y1 - pr->y0 + grow * 2};
}

static Prim* EmitPath(PaintCtx* ctx, Path* p, uint8_t kind, float grow) {
    PathRec* pr = PathOf(p);
    if (!pr) {
        return nullptr;
    }
    HashPath(*pr);
    Prim* prim = Emit(ctx, kind, PathBox(pr, grow));
    prim->path = (int32_t)((int)(uintptr_t)p - 1);
    return prim;
}

void RecPathFill(PaintCtx* ctx, Path* p, Rgba c) {
    Prim* prim = EmitPath(ctx, p, kPPathFill, 1);
    if (prim) {
        prim->color = PaintFade(ctx, c);
    }
}

void RecPathFillGradient(PaintCtx* ctx, Path* p, float x0, float y0, float x1,
                         float y1, Rgba from, Rgba to) {
    Prim* prim = EmitPath(ctx, p, kPPathGradient, 1);
    if (!prim) {
        return;
    }
    prim->e0 = x0;
    prim->e1 = y0;
    prim->e2 = x1;
    prim->e3 = y1;
    prim->color = PaintFade(ctx, from);
    prim->color2 = PaintFade(ctx, to);
}

void RecPathStroke(PaintCtx* ctx, Path* p, float stroke, Rgba c,
                   bool roundCaps) {
    Prim* prim = EmitPath(ctx, p, kPPathStroke, stroke > 0 ? stroke : 1);
    if (!prim) {
        return;
    }
    prim->e1 = stroke;
    prim->color = PaintFade(ctx, c);
    if (roundCaps) {
        prim->flags |= kFRoundCaps;
    }
}

void RecImageDraw(PaintCtx* ctx, Image* img, Bounds b, float radius) {
    Prim* p = Emit(ctx, kPImage, b);
    p->g0 = b.x;
    p->g1 = b.y;
    p->g2 = b.w;
    p->g3 = b.h;
    p->e0 = radius;
    p->ref = img;
}

void RecTextDraw(PaintCtx* ctx, TextLayout* tl, float x, float y, Rgba c,
                 bool clip, float clipW) {
    Size sz = TextLayoutSize(tl);
    Prim* p = Emit(ctx, kPText, Bounds{x, y, sz.w, sz.h});
    p->g0 = x;
    p->g1 = y;
    p->g2 = sz.w;
    p->g3 = sz.h;
    p->ref = tl;
    p->color = PaintFade(ctx, c);
    p->e0 = clipW;
    if (clip) {
        p->flags |= kFClip;
    }
}

// ─── ordering ────────────────────────────────────────────────────────────
//
// GPUI sorts its primitives by their order, which is the stacking context
// they were painted in; the position in the list breaks a tie. The tree here
// already paints its layers in two passes, so this is a stable sort that
// almost never moves anything — it is here because the layer is the thing
// that decides what covers what, and having it as a field rather than as the
// order of two walks is half of what a scene is.

static void SortByLayer(Vec<Prim>& v) {
    // Counting sort over the layer byte: a frame has a handful of layers and
    // thousands of primitives, and the pass has to be stable.
    int counts[256] = {};
    bool mixed = false;
    for (int i = 0; i < v.len; i++) {
        counts[v[i].layer]++;
        if (i > 0 && v[i].layer < v[i - 1].layer) {
            mixed = true;
        }
    }
    if (!mixed) {
        return;
    }
    int at = 0;
    int start[256] = {};
    for (int i = 0; i < 256; i++) {
        start[i] = at;
        at += counts[i];
    }
    Vec<Prim> out;
    VecAppendBlanks(out, v.len);
    for (int i = 0; i < v.len; i++) {
        out[start[v[i].layer]++] = v[i];
    }
    for (int i = 0; i < v.len; i++) {
        v[i] = out[i];
    }
    VecReset(out);
}

// ─── the path cache ──────────────────────────────────────────────────────
//
// A built backend path, kept across frames and found by the hash of the
// geometry that built it. This is where the D2D backend's frame time goes:
// filling a path means tessellating it, and a tessellation that can be
// realized once and drawn many times is the difference the measurements
// show.

struct CacheEntry {
    uint64_t hash = 0;
    Path* path = nullptr;
    int lastFrame = 0;
    bool live = false;
};

static const int kCacheSlots = 2048;
static CacheEntry gCache[kCacheSlots];
static int gCacheLive = 0;

static CacheEntry* CacheFind(uint64_t hash) {
    if (hash == 0) {
        hash = 1;
    }
    int at = (int)(hash % kCacheSlots);
    for (int i = 0; i < kCacheSlots; i++) {
        CacheEntry& e = gCache[at];
        if (!e.live) {
            return &e;
        }
        if (e.hash == hash) {
            return &e;
        }
        at = (at + 1) % kCacheSlots;
    }
    return nullptr;
}

static void CacheClear() {
    for (int i = 0; i < kCacheSlots; i++) {
        if (gCache[i].live && gCache[i].path) {
            PathFree(gCache[i].path);
        }
        gCache[i] = CacheEntry{};
    }
    gCacheLive = 0;
}

// Everything not asked for in the last `kCacheAge` frames goes. A rehash of
// the whole table, because open addressing cannot delete in place, and it
// only runs when the table is filling up.
static const int kCacheAge = 120;
static CacheEntry gSweepBuf[kCacheSlots];
static void CacheSweep() {
    CacheEntry* old = gSweepBuf;
    for (int i = 0; i < kCacheSlots; i++) {
        old[i] = gCache[i];
        gCache[i] = CacheEntry{};
    }
    gCacheLive = 0;
    for (int i = 0; i < kCacheSlots; i++) {
        if (!old[i].live) {
            continue;
        }
        if (gFrameNo - old[i].lastFrame > kCacheAge) {
            if (old[i].path) {
                PathFree(old[i].path);
            }
            continue;
        }
        CacheEntry* e = CacheFind(old[i].hash);
        if (e) {
            *e = old[i];
            gCacheLive++;
        } else if (old[i].path) {
            PathFree(old[i].path);
        }
    }
}

void Invalidate() {
    VecClear(gPrev);
    gHavePrev = false;
    gPrevFrameHash = 0;
    for (int i = 0; i < kBufferDepth; i++) {
        gDamageRing[i] = Bounds{};
    }
}

void Reset() {
    // The run's summary, on the way out. GPUI_FRAME_BENCH prints the same
    // counters for a benchmark, but a benchmark draws one frame over and
    // over: the partial-redraw numbers only mean anything when something on
    // screen actually moved, which is an interactive run.
    if (gStats.frames > 0) {
        logf(
            "scene: frames=%d unchanged=%d partial=%d meanDamage=%.1f%% "
            "cacheLive=%d",
            gStats.frames, gStats.framesUnchanged, gStats.framesPartial,
            gStats.framesPartial
                ? 100.0 * gStats.damageFracSum / gStats.framesPartial
                : 0.0,
            gCacheLive);
    }
    CacheClear();
    Invalidate();
}

// Build a backend path out of the recorded verbs. The caller is drawing, so
// Recording() is already false and these are the real entry points.
static Path* BuildPath(PaintCtx* ctx, const PathRec& pr) {
    Path* p = PathNew(ctx, pr.winding);
    if (!p) {
        return nullptr;
    }
    int vi = pr.verbFirst;
    int pi = pr.ptFirst;
    for (int i = 0; i < pr.verbCount; i++) {
        uint8_t v = gVerbs[vi++];
        switch (v & 0x7f) {
            case kVMove:
                PathMoveTo(p, gPts[pi], gPts[pi + 1]);
                pi += 2;
                break;
            case kVLine:
                PathLineTo(p, gPts[pi], gPts[pi + 1]);
                pi += 2;
                break;
            case kVCubic:
                PathCubicTo(p, gPts[pi], gPts[pi + 1], gPts[pi + 2],
                            gPts[pi + 3], gPts[pi + 4], gPts[pi + 5]);
                pi += 6;
                break;
            case kVArc:
                PathArcTo(p, gPts[pi], gPts[pi + 1], gPts[pi + 2], gPts[pi + 3],
                          gPts[pi + 4], (v & 0x80) != 0);
                pi += 5;
                break;
            case kVClose:
                PathClose(p);
                break;
        }
    }
    return p;
}

// The path a primitive draws, built or found. `owned` comes back true when
// the caller has to free it, which is every path at a level below `cache`.
static Path* PathFor(PaintCtx* ctx, const Prim& prim, bool* owned) {
    *owned = true;
    if (prim.path < 0 || prim.path >= gPaths.len) {
        return nullptr;
    }
    const PathRec& pr = gPaths[prim.path];
    if (SceneLevelOn() < kSceneCache) {
        gStats.pathCacheMisses++;
        return BuildPath(ctx, pr);
    }
    CacheEntry* e = CacheFind(pr.hash ? pr.hash : 1);
    if (e && e->live && e->hash == (pr.hash ? pr.hash : 1)) {
        e->lastFrame = gFrameNo;
        gStats.pathCacheHits++;
        *owned = false;
        return e->path;
    }
    Path* p = BuildPath(ctx, pr);
    gStats.pathCacheMisses++;
    if (!p) {
        return nullptr;
    }
    // Realizing costs something, which is why it happens here and not on
    // every path: a path worth caching is a path worth tessellating once.
    PathRealize(ctx, p);
    if (e && !e->live) {
        e->hash = pr.hash ? pr.hash : 1;
        e->path = p;
        e->lastFrame = gFrameNo;
        e->live = true;
        gCacheLive++;
        *owned = false;
    }
    return p;
}

// ─── diffing ─────────────────────────────────────────────────────────────
//
// Not position for position. Almost nothing that changes on screen leaves the
// primitive count alone — a hover adds a background fill, a row appears, a
// popup opens — and a diff that gives up when the counts differ gives up on
// nearly every frame that matters. So the two frames are compared as
// multisets of primitive hashes: what is in the new frame and not the old is
// damage, what was in the old and is not in the new is damage, and everything
// else did not move. A primitive that moved is both, which is right — the
// rectangle has to cover where it was and where it is.

// A hash to count map, open addressed, rebuilt each frame. Two of them, one
// per direction of the comparison.
struct HashBag {
    Vec<uint64_t> keys;
    Vec<int> counts;
    int mask = 0;
};
static HashBag gBagA;
static HashBag gBagB;

static void BagBuild(HashBag& b, const Vec<Prim>& v) {
    int cap = 16;
    while (cap < v.len * 2) {
        cap *= 2;
    }
    b.mask = cap - 1;
    VecClear(b.keys);
    VecClear(b.counts);
    VecAppendBlanks(b.keys, cap);
    VecAppendBlanks(b.counts, cap);
    for (int i = 0; i < cap; i++) {
        b.keys[i] = 0;
        b.counts[i] = 0;
    }
    for (int i = 0; i < v.len; i++) {
        uint64_t k = v[i].hash | 1; // 0 is the empty slot
        int at = (int)(k)&b.mask;
        while (b.counts[at] != 0 && b.keys[at] != k) {
            at = (at + 1) & b.mask;
        }
        b.keys[at] = k;
        b.counts[at]++;
    }
}

// One off the count if it is there, and true if it was.
static bool BagTake(HashBag& b, uint64_t hash) {
    uint64_t k = hash | 1;
    int at = (int)(k)&b.mask;
    for (int i = 0; i <= b.mask; i++) {
        if (b.counts[at] == 0 && b.keys[at] == 0) {
            return false;
        }
        if (b.keys[at] == k && b.counts[at] > 0) {
            b.counts[at]--;
            return true;
        }
        at = (at + 1) & b.mask;
    }
    return false;
}

bool FrameEnd(PaintCtx* ctx, Bounds* damage) {
    (void)ctx;
    gRecording = false;
    gFrameNo++;
    SortByLayer(gCur);

    uint64_t frameHash = kHashSeed;
    for (int i = 0; i < gCur.len; i++) {
        gCur[i].hash = HashPrim(gCur[i]);
        frameHash = HashBytes(frameHash, &gCur[i].hash, 8);
    }
    int nLayers = 0;
    {
        int last = -1;
        for (int i = 0; i < gCur.len; i++) {
            if (gCur[i].layer != last) {
                nLayers++;
                last = gCur[i].layer;
            }
        }
    }
    gStats.prims = gCur.len;
    gStats.layers = nLayers;
    gStats.pathPrims = 0;
    gStats.pathVerbs = gVerbs.len;
    for (int i = 0; i < gCur.len; i++) {
        if (gCur[i].kind >= kPPathFill && gCur[i].kind <= kPPathStroke) {
            gStats.pathPrims++;
        }
    }
    gStats.frames++;

    Bounds whole = Bounds{0, 0, gViewW, gViewH};
    Bounds dmg = whole;
    int changed = gCur.len;
    bool identical = false;
    if (gHavePrev) {
        if (frameHash == gPrevFrameHash && gPrev.len == gCur.len) {
            // Identical, which is worth knowing whether or not this level
            // acts on it. `dmg` stays the whole view: a level below `skip`
            // draws the frame again, and handing it an empty rectangle would
            // draw nothing at all.
            identical = true;
            changed = 0;
        }
    }
    if (!identical && gHavePrev && SceneLevelOn() >= kSceneDamage) {
        changed = 0;
        Bounds d = {};
        BagBuild(gBagA, gPrev);
        for (int i = 0; i < gCur.len; i++) {
            if (BagTake(gBagA, gCur[i].hash)) {
                continue;
            }
            changed++;
            d = Union(d, gCur[i].bbox);
        }
        BagBuild(gBagB, gCur);
        for (int i = 0; i < gPrev.len; i++) {
            if (BagTake(gBagB, gPrev[i].hash)) {
                continue;
            }
            changed++;
            d = Union(d, gPrev[i].bbox);
        }
        dmg = Intersect(d, whole);
    }
    gStats.primsChanged = changed;

    // The buffer this frame draws into holds what was presented three
    // presents ago, so the redraw has to cover that frame's damage too.
    if (SceneLevelOn() >= kSceneDamage && !identical) {
        Bounds acc = dmg;
        for (int i = 0; i < kBufferDepth; i++) {
            acc = Union(acc, gDamageRing[i]);
        }
        gDamageRing[gDamageRingAt] = dmg;
        gDamageRingAt = (gDamageRingAt + 1) % kBufferDepth;
        dmg = Intersect(acc, whole);
    }

    // The first few frames that differed, spelled out: a partial redraw is
    // the one thing a benchmark of one repeated frame cannot show, so an
    // interactive run has to be able to say that it happened and how big it
    // was.
    static int logged = 0;
    if (!identical && gHavePrev && logged < 8) {
        logged++;
        logf(
            "scene: changed %d of %d prims, damage %.0fx%.0f at %.0f,%.0f "
            "(view %.0fx%.0f)",
            changed, gCur.len, dmg.w, dmg.h, dmg.x, dmg.y, whole.w, whole.h);
    }

    bool skip = identical && SceneLevelOn() >= kSceneSkip;
    gSkipPresent = skip;
    if (identical) {
        gStats.framesUnchanged++;
    } else if (dmg.w < whole.w || dmg.h < whole.h) {
        gStats.framesPartial++;
        float area = whole.w * whole.h;
        gStats.damageFracSum += area > 0 ? (dmg.w * dmg.h) / area : 1.f;
    }

    // This frame becomes the one the next is compared against, whether or not
    // it was drawn: what is on screen did not change either way.
    VecClear(gPrev);
    for (int i = 0; i < gCur.len; i++) {
        VecAppend(gPrev, gCur[i]);
    }
    gPrevFrameHash = frameHash;
    gHavePrev = true;

    if (damage) {
        *damage = skip ? Bounds{0, 0, 0, 0} : dmg;
    }
    if (gCacheLive > kCacheSlots / 2) {
        CacheSweep();
    }
    gStats.pathCacheLive = gCacheLive;
    return !skip;
}

// ─── replay ──────────────────────────────────────────────────────────────

void Replay(PaintCtx* ctx, const Bounds* damage) {
    if (!ctx) {
        return;
    }
    Bounds whole = Bounds{0, 0, gViewW, gViewH};
    bool partial = damage && !SameBounds(*damage, whole);
    if (partial && (damage->w <= 0 || damage->h <= 0)) {
        return;
    }
    // The colours were faded as they were recorded, the way a backend fades
    // them as it is handed them; fading again would square the opacity.
    float saved = ctx->opacity;
    ctx->opacity = 1.f;
    if (partial) {
        CanvasPushClip(ctx, damage->x, damage->y, damage->w, damage->h);
    }
    Bounds cur = whole;
    bool pushed = false;
    gStats.maskChanges = 0;
    for (int i = 0; i < gCur.len; i++) {
        const Prim& p = gCur[i];
        if (p.bbox.w <= 0 || p.bbox.h <= 0) {
            // Wholly clipped away. A scene knows this; a tree walk that
            // issues the call does not.
            if (p.kind != kPClear) {
                gStats.culled++;
                continue;
            }
        }
        if (partial && p.kind != kPClear) {
            Bounds hit = Intersect(p.bbox, *damage);
            if (hit.w <= 0 || hit.h <= 0) {
                gStats.culled++;
                continue;
            }
        }
        if (!SameBounds(p.mask, cur)) {
            if (pushed) {
                CanvasPopClip(ctx);
                pushed = false;
            }
            if (!SameBounds(p.mask, whole)) {
                CanvasPushClip(ctx, p.mask.x, p.mask.y, p.mask.w, p.mask.h);
                pushed = true;
            }
            cur = p.mask;
            gStats.maskChanges++;
        }
        switch (p.kind) {
            case kPClear:
                if (partial) {
                    // Clearing is a fill of what is being redrawn: the rest
                    // of the buffer is last frame's and has to stay.
                    CanvasFillRect(ctx, damage->x, damage->y, damage->w,
                                   damage->h, p.color);
                } else {
                    CanvasClear(ctx, p.color);
                }
                break;
            case kPRect:
                CanvasFillRect(ctx, p.g0, p.g1, p.g2, p.g3, p.color);
                break;
            case kPRound:
                CanvasFillRound(ctx, p.g0, p.g1, p.g2, p.g3, p.e0, p.color);
                break;
            case kPStrokeRound: {
                float dash[2] = {p.e2, p.e3};
                CanvasStrokeRound(ctx, p.g0, p.g1, p.g2, p.g3, p.e0, p.e1,
                                  p.color, (p.flags & kFDash) ? dash : nullptr);
                break;
            }
            case kPLine: {
                float dash[2] = {p.e2, p.e3};
                CanvasLine(ctx, p.g0, p.g1, p.g2, p.g3, p.e1, p.color,
                           (p.flags & kFDash) ? dash : nullptr);
                break;
            }
            case kPEllipse:
                CanvasEllipse(ctx, p.g0, p.g1, p.g2, p.g3, p.e1, p.color);
                break;
            case kPImage:
                ImageDraw(ctx, (Image*)p.ref, Bounds{p.g0, p.g1, p.g2, p.g3},
                          p.e0);
                break;
            case kPText:
                TextLayoutDraw(ctx, (TextLayout*)p.ref, p.g0, p.g1, p.color,
                               (p.flags & kFClip) != 0, p.e0);
                break;
            case kPPathFill:
            case kPPathGradient:
            case kPPathStroke: {
                bool owned = false;
                Path* path = PathFor(ctx, p, &owned);
                if (!path) {
                    break;
                }
                if (p.kind == kPPathFill) {
                    PathFill(ctx, path, p.color);
                } else if (p.kind == kPPathGradient) {
                    PathFillGradient(ctx, path, p.e0, p.e1, p.e2, p.e3, p.color,
                                     p.color2);
                } else {
                    PathStroke(ctx, path, p.e1, p.color,
                               (p.flags & kFRoundCaps) != 0);
                }
                if (owned) {
                    PathFree(path);
                }
                break;
            }
        }
    }
    if (pushed) {
        CanvasPopClip(ctx);
    }
    if (partial) {
        CanvasPopClip(ctx);
    }
    ctx->opacity = saved;
}

} // namespace scene
} // namespace gpui
