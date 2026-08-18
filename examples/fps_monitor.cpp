#include "gpui.h"

using namespace gpui;

#include <math.h>

static const float kHilbertSize = 200.f;
static const uint32_t kHilbertIter = 1;
static const int kSubdiv = 6;
static const float kHilbertExtent = kHilbertSize / 2.f * 1.5f;
static const float kCellFill = 0.68f;
static const int kSegPts = 6;
static const int kMaxCurves = 48;
static const float kEye = 620.f;
static const float kEase = 0.08f;
static const int kMaxPts = 512;
static const int kFpsHist = 120;

struct V3 {
    float x = 0, y = 0, z = 0;
};

struct FpsApp {
    static El* Render(FpsApp* self, Ctx* cx);
    V3 pts[kMaxPts];
    int nPts = 0;
    Rgba pal[3][kMaxPts];
    int curves = 6;
    float cursorX = 0, cursorY = 0;
    float tiltX = 0, tiltY = 0;
    ULONGLONG start = 0;
    float hist[kFpsHist] = {};
    int histN = 0;
    ULONGLONG lastTick = 0;
};

static float Dist(V3 a, V3 b) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

static V3 Lerp(V3 a, V3 b, float t) {
    return V3{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
              a.z + (b.z - a.z) * t};
}

static void Hilbert(V3 center, float size, uint32_t iter, int v[8], V3* out,
                    int* n) {
    float half = size / 2.f;
    V3 corners[8] = {
        {center.x - half, center.y + half, center.z - half},
        {center.x - half, center.y + half, center.z + half},
        {center.x - half, center.y - half, center.z + half},
        {center.x - half, center.y - half, center.z - half},
        {center.x + half, center.y - half, center.z - half},
        {center.x + half, center.y - half, center.z + half},
        {center.x + half, center.y + half, center.z + half},
        {center.x + half, center.y + half, center.z - half},
    };
    V3 vec[8];
    for (int i = 0; i < 8; i++) {
        vec[i] = corners[v[i]];
    }
    if (iter == 0) {
        for (int i = 0; i < 8 && *n < kMaxPts; i++) {
            out[(*n)++] = vec[i];
        }
        return;
    }
    int children[8][8] = {
        {v[0], v[3], v[4], v[7], v[6], v[5], v[2], v[1]},
        {v[0], v[7], v[6], v[1], v[2], v[5], v[4], v[3]},
        {v[0], v[7], v[6], v[1], v[2], v[5], v[4], v[3]},
        {v[2], v[3], v[0], v[1], v[6], v[7], v[4], v[5]},
        {v[2], v[3], v[0], v[1], v[6], v[7], v[4], v[5]},
        {v[4], v[3], v[2], v[5], v[6], v[1], v[0], v[7]},
        {v[4], v[3], v[2], v[5], v[6], v[1], v[0], v[7]},
        {v[6], v[5], v[2], v[1], v[0], v[3], v[4], v[7]},
    };
    for (int i = 0; i < 8; i++) {
        Hilbert(vec[i], half, iter - 1, children[i], out, n);
    }
}

static V3 Catmull(V3* ctrl, int nCtrl, float t) {
    if (nCtrl <= 0) {
        return {};
    }
    if (nCtrl == 1) {
        return ctrl[0];
    }
    int spans = nCtrl - 1;
    float scaled = t * (float)spans;
    if (scaled < 0) {
        scaled = 0;
    }
    if (scaled > (float)spans) {
        scaled = (float)spans;
    }
    int span = (int)scaled;
    if (span > spans - 1) {
        span = spans - 1;
    }
    float local = scaled - (float)span;
    auto at = [&](int i) -> V3 {
        if (i < 0) {
            i = 0;
        }
        if (i >= nCtrl) {
            i = nCtrl - 1;
        }
        return ctrl[i];
    };
    V3 p0 = at(span - 1), p1 = at(span), p2 = at(span + 1), p3 = at(span + 2);
    auto knot = [](V3 a, V3 b) {
        float d = sqrtf(Dist(a, b));
        return d < 1e-3f ? 1e-3f : d;
    };
    float t0 = 0, t1 = t0 + knot(p0, p1), t2 = t1 + knot(p1, p2),
          t3 = t2 + knot(p2, p3);
    float tt = t1 + (t2 - t1) * local;
    V3 a1 = Lerp(p0, p1, (tt - t0) / (t1 - t0));
    V3 a2 = Lerp(p1, p2, (tt - t1) / (t2 - t1));
    V3 a3 = Lerp(p2, p3, (tt - t3 + t3 - t2) / (t3 - t2));
    a3 = Lerp(p2, p3, (tt - t2) / (t3 - t2));
    V3 b1 = Lerp(a1, a2, (tt - t0) / (t2 - t0));
    V3 b2 = Lerp(a2, a3, (tt - t1) / (t3 - t1));
    return Lerp(b1, b2, (tt - t1) / (t2 - t1));
}

static Rgba Hsla(float h, float s, float l) {
    // h 0..1, s/l 0..1 -> rgb
    float c = (1 - fabsf(2 * l - 1)) * s;
    float hp = h * 6.f;
    float x = c * (1 - fabsf(fmodf(hp, 2.f) - 1));
    float r = 0, g = 0, b = 0;
    if (hp < 1) {
        r = c;
        g = x;
    } else if (hp < 2) {
        r = x;
        g = c;
    } else if (hp < 3) {
        g = c;
        b = x;
    } else if (hp < 4) {
        g = x;
        b = c;
    } else if (hp < 5) {
        r = x;
        b = c;
    } else {
        r = c;
        b = x;
    }
    float m = l - c * 0.5f;
    return Rgb((uint8_t)((r + m) * 255), (uint8_t)((g + m) * 255),
               (uint8_t)((b + m) * 255));
}

static void BuildGeom(FpsApp* app) {
    V3 ctrl[64];
    int nCtrl = 0;
    int order[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    Hilbert({}, kHilbertSize, kHilbertIter, order, ctrl, &nCtrl);
    int samples = nCtrl * kSubdiv;
    if (samples > kMaxPts - 1) {
        samples = kMaxPts - 1;
    }
    app->nPts = 0;
    for (int i = 0; i <= samples && app->nPts < kMaxPts; i++) {
        app->pts[app->nPts++] = Catmull(ctrl, nCtrl, (float)i / (float)samples);
    }
    for (int i = 0; i < app->nPts; i++) {
        V3 v = app->pts[i];
        float lx = (-v.x / 200.f);
        if (lx < 0) {
            lx = 0;
        }
        float ly = (-v.y / 200.f);
        if (ly < 0) {
            ly = 0;
        }
        app->pal[0][i] = Hsla(0.6f, 1.f, lx + 0.5f);
        app->pal[1][i] = Hsla(0.9f, 1.f, ly + 0.5f);
        app->pal[2][i] = Hsla((float)i / (float)app->nPts, 1.f, 0.5f);
    }
}

static void Project(V3 v, float yaw, float pitch, float cx, float cy,
                    float scale, float* ox, float* oy) {
    float sy = sinf(yaw), cyw = cosf(yaw);
    float x = v.x * cyw + v.z * sy;
    float z = v.z * cyw - v.x * sy;
    float sp = sinf(pitch), cp = cosf(pitch);
    float y = v.y * cp - z * sp;
    z = z * cp + v.y * sp;
    float depth = kEye + z;
    if (depth < 1) {
        depth = 1;
    }
    float persp = kEye / depth;
    *ox = cx + x * persp * scale;
    *oy = cy + y * persp * scale;
}

static void PaintCurves(PaintCtx* ctx, El* e, void* user) {
    auto* app = (FpsApp*)user;
    float w = e->w, h = e->h;
    int curves = app->curves;
    int columns = (int)ceilf(sqrtf((float)curves));
    if (columns < 1) {
        columns = 1;
    }
    int rows = (curves + columns - 1) / columns;
    float cellW = w / (float)columns;
    float cellH = h / (float)rows;
    float scale =
        (cellW < cellH ? cellW : cellH) / (kHilbertExtent * 2.f) * kCellFill;
    ULONGLONG now = GetTickCount64();
    float spin = (float)(now - app->start) / 1000.f * 0.35f;

    for (int index = 0; index < curves; index++) {
        int col = index % columns;
        int row = index / columns;
        float cx = e->x + cellW * (col + 0.5f);
        float cy = e->y + cellH * (row + 0.5f);
        float dir = (index % 2 == 0) ? 1.f : -1.f;
        float yaw = spin * dir + index * 0.4f + app->tiltX;
        float pitch = app->tiltY;
        const Rgba* pal = app->pal[index % 3];
        float px[kMaxPts], py[kMaxPts];
        for (int i = 0; i < app->nPts; i++) {
            Project(app->pts[i], yaw, pitch, cx, cy, scale, &px[i], &py[i]);
        }
        int start = 0;
        while (start + 1 < app->nPts) {
            int end = start + kSegPts;
            if (end > app->nPts - 1) {
                end = app->nPts - 1;
            }
            Rgba col = pal[(start + end) / 2];
            if (ctx->brush) {
                ctx->brush->SetColor(RgbaToD2D(col));
            }
            for (int i = start + 1; i <= end; i++) {
                ctx->rt->DrawLine(D2D1::Point2F(px[i - 1], py[i - 1]),
                                  D2D1::Point2F(px[i], py[i]), ctx->brush, 1.f);
            }
            start = end;
        }
    }
}

static void OnTick(FpsApp* app, Ctx* cx, const TickEvent*) {
    ULONGLONG now = GetTickCount64();
    float dt = (float)(now - app->lastTick);
    app->lastTick = now;
    if (dt < 1) {
        dt = 1;
    }
    float fps = 1000.f / dt;
    if (app->histN < kFpsHist) {
        app->hist[app->histN++] = fps;
    } else {
        memmove(app->hist, app->hist + 1, sizeof(float) * (kFpsHist - 1));
        app->hist[kFpsHist - 1] = fps;
    }
    app->tiltX += (app->cursorX - app->tiltX) * kEase;
    app->tiltY += (app->cursorY - app->tiltY) * kEase;
}

static void OnClick(FpsApp* app, Ctx* cx, const ClickEvent* ev) {
    (void)app;
    (void)cx;
    (void)ev;
}

static void OnMouse(FpsApp* app, Ctx* cx, const ClickEvent* ev) {
    Window* win = cx->win;

    float x = ev->x;
    float y = ev->y;
    RECT rc = {};
    GetClientRect(cx->win->hwnd, &rc);
    float w = (float)(rc.right - rc.left);
    float h = (float)(rc.bottom - rc.top);
    if (w <= 0 || h <= 0) {
        return;
    }
    app->cursorX = (x / w - 0.5f) * 2.4f;
    app->cursorY = (y / h - 0.5f) * 1.2f;
}

static void StepCurves(FpsApp* app, Ctx* cx, const ClickEvent*,
                       intptr_t delta) {
    int n = app->curves + (int)delta;
    if (n >= 1 && n <= kMaxCurves) {
        app->curves = n;
    }
    Notify(cx);
}

El* FpsApp::Render(FpsApp* app, Ctx* cx) {
    Arena* frame = cx->a;
    Window* win = cx->win;

    WinSize size = WindowSize(cx->win);

    El* canvas = Div(frame)->SizeFull();
    canvas->customPaint = PaintCurves;
    canvas->customUser = app;

    float fps = app->histN ? app->hist[app->histN - 1] : 0;
    El* hud = Div(frame)
                  ->Absolute()
                  ->Top(8)
                  ->Right(8)
                  ->PadX(10)
                  ->PadY(6)
                  ->Radius(6)
                  ->Bg(Rgba8(0, 0, 0, 160))
                  ->Child(TextEl(frame, fmt("%.0f FPS", fps))
                              ->Font(12)
                              ->Fg(Rgb(200, 200, 200)));

    El* ctrl = Div(frame)
                   ->Absolute()
                   ->Bottom(16)
                   ->Left(16)
                   ->FlexRow()
                   ->ItemsCenter()
                   ->Gap(8)
                   ->Child(ButtonEl(frame, 1, StrL("- load"))
                               ->OnClick(Listen(cx, &StepCurves, -1)))
                   ->Child(TextEl(frame, fmt("%d curves", app->curves))
                               ->Font(12)
                               ->Fg(Rgb(190, 190, 190)))
                   ->Child(ButtonEl(frame, 2, StrL("+ load"))
                               ->OnClick(Listen(cx, &StepCurves, 1)));

    return Div(frame)
        ->SizeFull()
        ->Bg(Rgb(0, 0, 0))
        ->Child(canvas)
        ->Child(hud)
        ->Child(ctrl);
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    App* app = AppNew();
    Entity<FpsApp> view = EntityNew<FpsApp>(app);
    FpsApp* self = view.Get(app);
    (void)self;
    ThemeSet(ThemeMode::Dark);
    self->curves = 6;
    self->start = GetTickCount64();
    self->lastTick = self->start;
    BuildGeom(self);
    WinOpts opts = {};
    opts.anim = true;
    opts.timerMs = 16;
    Window* win =
        WindowOpenView(app, StrL("FPS Monitor"), 800, 600, view.id, opts);
    WindowOnClick(win, ListenTo(view, &OnClick));
    WindowOnMouse(win, ListenTo(view, &OnMouse));
    WindowSetInterval(win, 16, ListenTo(view, &OnTick));
    int rc = AppRun(app);
    AppFree(app);
    return rc;
}
