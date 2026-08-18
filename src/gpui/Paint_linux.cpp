/* cairo + Pango backend for Paint.h.

   cairo's user space is y-down with clockwise-increasing angles, the same
   convention the element tree uses, so nothing here flips coordinates. */

#include "gpui/Paint.h"

#include <math.h>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>

namespace gpui {

static const float kPi = 3.14159265358979f;

struct PaintApp {
    // A cairo-backed font map context, so a layout can be shaped and measured
    // without a target bound.
    PangoContext* pango = nullptr;
};

struct PaintTarget {
    cairo_t* cr = nullptr;
};

// ─── lifecycle ────────────────────────────────────────────────────────────

PaintApp* PaintAppNew() {
    auto* pa = new PaintApp();
    PangoFontMap* map = pango_cairo_font_map_get_default();
    if (!map) {
        delete pa;
        return nullptr;
    }
    pa->pango = pango_font_map_create_context(map);
    if (!pa->pango) {
        delete pa;
        return nullptr;
    }
    // Grayscale AA, not the fontconfig default of subpixel: a frame is drawn
    // into an image surface and blitted, so LCD filtering would bake color
    // fringes into text that DirectWrite does not produce.
    cairo_font_options_t* fo = cairo_font_options_create();
    cairo_font_options_set_antialias(fo, CAIRO_ANTIALIAS_GRAY);
    cairo_font_options_set_hint_style(fo, CAIRO_HINT_STYLE_SLIGHT);
    cairo_font_options_set_hint_metrics(fo, CAIRO_HINT_METRICS_OFF);
    pango_cairo_context_set_font_options(pa->pango, fo);
    cairo_font_options_destroy(fo);
    // Sizes are set in absolute device units, so keep the context at 1:1.
    pango_cairo_context_set_resolution(pa->pango, 96.0);
    return pa;
}

void PaintAppFree(PaintApp* pa) {
    if (!pa) {
        return;
    }
    if (pa->pango) {
        g_object_unref(pa->pango);
    }
    delete pa;
}

void PaintTargetFree(PaintCtx* ctx) {
    if (!ctx || !ctx->rt) {
        return;
    }
    if (ctx->rt->cr) {
        cairo_destroy(ctx->rt->cr);
    }
    delete ctx->rt;
    ctx->rt = nullptr;
}

// `native` is the window's cairo_surface_t; the context is per-frame because
// an XCB/Xlib surface is resized under us between expose events.
bool PaintTargetBegin(PaintCtx* ctx, void* native, int pxW, int pxH) {
    (void)pxW;
    (void)pxH;
    if (!ctx || !ctx->pa || !native) {
        return false;
    }
    PaintTargetFree(ctx);
    auto* t = new PaintTarget();
    t->cr = cairo_create((cairo_surface_t*)native);
    if (!t->cr || cairo_status(t->cr) != CAIRO_STATUS_SUCCESS) {
        if (t->cr) {
            cairo_destroy(t->cr);
        }
        delete t;
        return false;
    }
    ctx->rt = t;
    cairo_set_antialias(t->cr, CAIRO_ANTIALIAS_DEFAULT);
    return true;
}

bool PaintTargetEnd(PaintCtx* ctx) {
    if (!ctx || !ctx->rt || !ctx->rt->cr) {
        return false;
    }
    cairo_surface_t* surf = cairo_get_target(ctx->rt->cr);
    if (surf) {
        cairo_surface_flush(surf);
    }
    PaintTargetFree(ctx);
    return true;
}

// ─── canvas ───────────────────────────────────────────────────────────────

static cairo_t* Cr(PaintCtx* ctx) {
    return (ctx && ctx->rt) ? ctx->rt->cr : nullptr;
}

static void SetColor(cairo_t* cr, Rgba c) {
    cairo_set_source_rgba(cr, c.r / 255.0, c.g / 255.0, c.b / 255.0,
                          c.a / 255.0);
}

// D2D measures a dash pattern in stroke widths; cairo in user units.
static void SetDash(cairo_t* cr, const float* dash, float stroke) {
    if (!dash) {
        cairo_set_dash(cr, nullptr, 0, 0);
        return;
    }
    double d[2] = {dash[0] * stroke, dash[1] * stroke};
    cairo_set_dash(cr, d, 2, 0);
}

static void RoundRectPath(cairo_t* cr, float x, float y, float w, float h,
                          float r) {
    float rmax = (w < h ? w : h) * 0.5f;
    if (r > rmax) {
        r = rmax;
    }
    if (r <= 0) {
        cairo_rectangle(cr, x, y, w, h);
        return;
    }
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -kPi / 2, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, kPi / 2);
    cairo_arc(cr, x + r, y + h - r, r, kPi / 2, kPi);
    cairo_arc(cr, x + r, y + r, r, kPi, 3 * kPi / 2);
    cairo_close_path(cr);
}

void CanvasClear(PaintCtx* ctx, Rgba c) {
    cairo_t* cr = Cr(ctx);
    if (!cr) {
        return;
    }
    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    SetColor(cr, c);
    cairo_paint(cr);
    cairo_restore(cr);
}

void CanvasFillRect(PaintCtx* ctx, float x, float y, float w, float h, Rgba c) {
    cairo_t* cr = Cr(ctx);
    if (!cr || w <= 0 || h <= 0 || c.a == 0) {
        return;
    }
    SetColor(cr, c);
    cairo_rectangle(cr, x, y, w, h);
    cairo_fill(cr);
}

void CanvasFillRound(PaintCtx* ctx, float x, float y, float w, float h, float r,
                     Rgba c) {
    cairo_t* cr = Cr(ctx);
    if (!cr || w <= 0 || h <= 0 || c.a == 0) {
        return;
    }
    SetColor(cr, c);
    RoundRectPath(cr, x, y, w, h, r);
    cairo_fill(cr);
}

void CanvasStrokeRound(PaintCtx* ctx, float x, float y, float w, float h,
                       float r, float stroke, Rgba c, const float* dash) {
    cairo_t* cr = Cr(ctx);
    if (!cr || stroke <= 0 || w <= 0 || h <= 0) {
        return;
    }
    SetColor(cr, c);
    cairo_set_line_width(cr, stroke);
    SetDash(cr, dash, stroke);
    // Inset by half the stroke: cairo, like D2D, centers it on the path.
    RoundRectPath(cr, x + stroke * 0.5f, y + stroke * 0.5f, w - stroke,
                  h - stroke, r);
    cairo_stroke(cr);
    cairo_set_dash(cr, nullptr, 0, 0);
}

void CanvasLine(PaintCtx* ctx, float x1, float y1, float x2, float y2,
                float stroke, Rgba c, const float* dash) {
    cairo_t* cr = Cr(ctx);
    if (!cr) {
        return;
    }
    SetColor(cr, c);
    cairo_set_line_width(cr, stroke);
    SetDash(cr, dash, stroke);
    cairo_move_to(cr, x1, y1);
    cairo_line_to(cr, x2, y2);
    cairo_stroke(cr);
    cairo_set_dash(cr, nullptr, 0, 0);
}

void CanvasEllipse(PaintCtx* ctx, float cx, float cy, float rx, float ry,
                   float stroke, Rgba c) {
    cairo_t* cr = Cr(ctx);
    if (!cr || rx <= 0 || ry <= 0) {
        return;
    }
    SetColor(cr, c);
    cairo_save(cr);
    cairo_translate(cr, cx, cy);
    cairo_scale(cr, rx, ry);
    cairo_new_sub_path(cr);
    cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
    cairo_restore(cr);
    if (stroke > 0) {
        cairo_set_line_width(cr, stroke);
        cairo_stroke(cr);
    } else {
        cairo_fill(cr);
    }
}

void CanvasPushClip(PaintCtx* ctx, float x, float y, float w, float h) {
    cairo_t* cr = Cr(ctx);
    if (!cr) {
        return;
    }
    cairo_save(cr);
    cairo_rectangle(cr, x, y, w, h);
    cairo_clip(cr);
}

void CanvasPopClip(PaintCtx* ctx) {
    cairo_t* cr = Cr(ctx);
    if (cr) {
        cairo_restore(cr);
    }
}

// ─── paths ────────────────────────────────────────────────────────────────
//
// cairo builds its path on the context, but Paint.h hands a Path around
// before there is anything to draw it on, so the ops are recorded and
// replayed at fill / stroke time.

enum PathCmd : uint8_t {
    kPathMove,
    kPathLine,
    kPathCubic,
    kPathArc,
    kPathClose
};

struct PathOp {
    PathCmd cmd = kPathMove;
    bool clockwise = false;
    float a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;
};

struct Path {
    Vec<PathOp> ops;
    bool winding = true;
    bool fig = false;
};

Path* PathNew(PaintCtx* ctx, bool winding) {
    if (!ctx) {
        return nullptr;
    }
    auto* p = new Path();
    p->winding = winding;
    return p;
}

void PathFree(Path* p) {
    delete p;
}

static void Push(Path* p, const PathOp& op) {
    if (p) {
        p->ops.Append(op);
    }
}

void PathMoveTo(Path* p, float x, float y) {
    if (!p) {
        return;
    }
    PathOp op;
    op.cmd = kPathMove;
    op.a = x;
    op.b = y;
    Push(p, op);
    p->fig = true;
}

void PathLineTo(Path* p, float x, float y) {
    if (!p) {
        return;
    }
    if (!p->fig) {
        PathMoveTo(p, x, y);
        return;
    }
    PathOp op;
    op.cmd = kPathLine;
    op.a = x;
    op.b = y;
    Push(p, op);
}

void PathCubicTo(Path* p, float x1, float y1, float x2, float y2, float x,
                 float y) {
    if (!p) {
        return;
    }
    if (!p->fig) {
        PathMoveTo(p, x, y);
        return;
    }
    PathOp op;
    op.cmd = kPathCubic;
    op.a = x1;
    op.b = y1;
    op.c = x2;
    op.d = y2;
    op.e = x;
    op.f = y;
    Push(p, op);
}

void PathArcTo(Path* p, float cx, float cy, float r, float a0, float a1,
               bool clockwise) {
    if (!p) {
        return;
    }
    PathOp op;
    op.cmd = kPathArc;
    op.clockwise = clockwise;
    op.a = cx;
    op.b = cy;
    op.c = r;
    op.d = a0;
    op.e = a1;
    Push(p, op);
    // cairo_arc draws a line from the current point to the arc start, so an
    // arc opens a figure the same way a move does.
    p->fig = true;
}

void PathClose(Path* p) {
    if (!p || !p->fig) {
        return;
    }
    PathOp op;
    op.cmd = kPathClose;
    Push(p, op);
    p->fig = false;
}

static bool Replay(cairo_t* cr, Path* p) {
    if (!cr || !p || p->ops.len == 0) {
        return false;
    }
    cairo_new_path(cr);
    cairo_set_fill_rule(
        cr, p->winding ? CAIRO_FILL_RULE_WINDING : CAIRO_FILL_RULE_EVEN_ODD);
    for (int i = 0; i < p->ops.len; i++) {
        const PathOp& o = p->ops[i];
        switch (o.cmd) {
            case kPathMove:
                cairo_move_to(cr, o.a, o.b);
                break;
            case kPathLine:
                cairo_line_to(cr, o.a, o.b);
                break;
            case kPathCubic:
                cairo_curve_to(cr, o.a, o.b, o.c, o.d, o.e, o.f);
                break;
            case kPathArc:
                if (o.clockwise) {
                    cairo_arc(cr, o.a, o.b, o.c, o.d, o.e);
                } else {
                    cairo_arc_negative(cr, o.a, o.b, o.c, o.d, o.e);
                }
                break;
            case kPathClose:
                cairo_close_path(cr);
                break;
        }
    }
    return true;
}

void PathFill(PaintCtx* ctx, Path* p, Rgba c) {
    cairo_t* cr = Cr(ctx);
    if (!Replay(cr, p)) {
        return;
    }
    SetColor(cr, c);
    cairo_fill(cr);
}

void PathFillGradientV(PaintCtx* ctx, Path* p, float y0, float y1, Rgba top,
                       Rgba bot) {
    cairo_t* cr = Cr(ctx);
    if (!Replay(cr, p)) {
        return;
    }
    cairo_pattern_t* pat = cairo_pattern_create_linear(0, y0, 0, y1);
    if (!pat) {
        SetColor(cr, top);
        cairo_fill(cr);
        return;
    }
    cairo_pattern_add_color_stop_rgba(pat, 0, top.r / 255.0, top.g / 255.0,
                                      top.b / 255.0, top.a / 255.0);
    cairo_pattern_add_color_stop_rgba(pat, 1, bot.r / 255.0, bot.g / 255.0,
                                      bot.b / 255.0, bot.a / 255.0);
    cairo_set_source(cr, pat);
    cairo_fill(cr);
    cairo_pattern_destroy(pat);
}

void PathStroke(PaintCtx* ctx, Path* p, float stroke, Rgba c, bool roundCaps) {
    cairo_t* cr = Cr(ctx);
    if (!Replay(cr, p)) {
        return;
    }
    SetColor(cr, c);
    cairo_set_line_width(cr, stroke);
    cairo_set_line_cap(cr,
                       roundCaps ? CAIRO_LINE_CAP_ROUND : CAIRO_LINE_CAP_BUTT);
    cairo_set_line_join(
        cr, roundCaps ? CAIRO_LINE_JOIN_ROUND : CAIRO_LINE_JOIN_MITER);
    cairo_stroke(cr);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);
}

// ─── shaped text ──────────────────────────────────────────────────────────
//
// Pango indexes by UTF-8 byte, which is what Str carries, so unlike the
// DirectWrite backend nothing has to convert offsets.

struct TextLayout {
    PangoLayout* layout = nullptr;
    int refs = 1;
    // GPUI's line box (fontSize * phi) and what Pango would use on its own.
    float box = 0;
    float natural = 0;
    int lines = 1;
};

// DirectWrite gets "Segoe UI" and "Consolas"; fontconfig resolves these two
// generic families to whatever the distro ships.
static const char* kSans = "Sans";
static const char* kMono = "Monospace";

static PangoWeight PangoWeightFor(uint8_t weight, float fontSize) {
    switch (weight & kFontWeightMask) {
        case kFontWeightBold:
            return PANGO_WEIGHT_BOLD;
        case kFontWeightSemibold:
            return PANGO_WEIGHT_SEMIBOLD;
        case kFontWeightMedium:
            return PANGO_WEIGHT_MEDIUM;
        default:
            break;
    }
    // The 20 px and 24 px DirectWrite formats are created semibold, and a run
    // that asks for no weight of its own inherits that. Match it.
    return fontSize >= 18.f ? PANGO_WEIGHT_SEMIBOLD : PANGO_WEIGHT_NORMAL;
}

TextLayout* TextLayoutNew(PaintCtx* ctx, Str s, float fontSize, float maxW,
                          bool wrap, uint8_t weight, float lineH, float* outW,
                          float* outH) {
    if (!ctx || !ctx->pa || !ctx->pa->pango || !s.s || s.len <= 0) {
        return nullptr;
    }
    if (fontSize <= 0) {
        fontSize = 16.f;
    }
    PangoLayout* l = pango_layout_new(ctx->pa->pango);
    if (!l) {
        return nullptr;
    }
    PangoFontDescription* fd = pango_font_description_new();
    pango_font_description_set_family(fd, (weight & kFontMono) ? kMono : kSans);
    pango_font_description_set_weight(fd, PangoWeightFor(weight, fontSize));
    if (weight & kFontItalic) {
        pango_font_description_set_style(fd, PANGO_STYLE_ITALIC);
    }
    pango_font_description_set_absolute_size(fd,
                                             (double)fontSize * PANGO_SCALE);
    pango_layout_set_font_description(l, fd);
    pango_font_description_free(fd);

    if (weight & kFontUnderline) {
        PangoAttrList* attrs = pango_attr_list_new();
        pango_attr_list_insert(
            attrs, pango_attr_underline_new(PANGO_UNDERLINE_SINGLE));
        pango_layout_set_attributes(l, attrs);
        pango_attr_list_unref(attrs);
    }

    pango_layout_set_text(l, s.s, s.len);
    if (wrap && maxW > 0) {
        pango_layout_set_width(l, (int)(maxW * PANGO_SCALE));
        pango_layout_set_wrap(l, PANGO_WRAP_WORD_CHAR);
    } else {
        pango_layout_set_width(l, -1);
    }

    auto* tl = new TextLayout();
    tl->layout = l;
    tl->lines = pango_layout_get_line_count(l);
    if (tl->lines < 1) {
        tl->lines = 1;
    }
    int pw = 0, ph = 0;
    pango_layout_get_pixel_size(l, &pw, &ph);
    tl->natural = (float)ph / (float)tl->lines;
    tl->box = fontSize * (lineH > 0 ? lineH : kLineHeight);
    // Pango's spacing only goes between lines, so the extra half-box above the
    // first line and below the last is added by the caller-visible height and
    // by the draw offset in TextLayoutDraw.
    if (tl->lines > 1) {
        pango_layout_set_spacing(l,
                                 (int)((tl->box - tl->natural) * PANGO_SCALE));
        pango_layout_get_pixel_size(l, &pw, &ph);
    }
    if (outW) {
        *outW = (float)pw;
    }
    if (outH) {
        *outH = tl->box * (float)tl->lines;
    }
    return tl;
}

void TextLayoutAddRef(TextLayout* tl) {
    if (tl) {
        tl->refs++;
    }
}

void TextLayoutRelease(TextLayout* tl) {
    if (!tl) {
        return;
    }
    if (--tl->refs > 0) {
        return;
    }
    if (tl->layout) {
        g_object_unref(tl->layout);
    }
    delete tl;
}

// Where the glyphs sit inside the phi-tall line box.
static float BoxPad(TextLayout* tl) {
    return (tl->box - tl->natural) * 0.5f;
}

void TextLayoutDraw(PaintCtx* ctx, TextLayout* tl, float x, float y, Rgba c,
                    bool clip) {
    cairo_t* cr = Cr(ctx);
    if (!cr || !tl || !tl->layout) {
        return;
    }
    if (clip) {
        int pw = 0, ph = 0;
        pango_layout_get_pixel_size(tl->layout, &pw, &ph);
        int w = pango_layout_get_width(tl->layout);
        float boxW = w > 0 ? (float)w / PANGO_SCALE : (float)pw;
        cairo_save(cr);
        cairo_rectangle(cr, x, y, boxW, tl->box * (float)tl->lines);
        cairo_clip(cr);
    }
    SetColor(cr, c);
    cairo_move_to(cr, x, y + BoxPad(tl));
    pango_cairo_show_layout(cr, tl->layout);
    if (clip) {
        cairo_restore(cr);
    }
}

int TextLayoutHitPoint(TextLayout* tl, Str s, float relX, float relY) {
    if (!tl || !tl->layout) {
        return 0;
    }
    int index = 0;
    int trailing = 0;
    pango_layout_xy_to_index(tl->layout, (int)(relX * PANGO_SCALE),
                             (int)((relY - BoxPad(tl)) * PANGO_SCALE), &index,
                             &trailing);
    // `trailing` counts characters past `index` the point fell after.
    const char* text = pango_layout_get_text(tl->layout);
    while (trailing > 0 && text && text[index]) {
        index = (int)(g_utf8_next_char(text + index) - text);
        trailing--;
    }
    if (index < 0) {
        index = 0;
    }
    if (index > s.len) {
        index = s.len;
    }
    return index;
}

int TextLayoutRangeRects(TextLayout* tl, Str s, int u8a, int u8b, RectF* out,
                         int max) {
    if (!tl || !tl->layout || !out || max <= 0 || u8a >= u8b) {
        return 0;
    }
    (void)s;
    float pad = BoxPad(tl);
    int n = 0;
    PangoLayoutIter* iter = pango_layout_get_iter(tl->layout);
    if (!iter) {
        return 0;
    }
    do {
        PangoLayoutLine* line = pango_layout_iter_get_line_readonly(iter);
        if (!line) {
            continue;
        }
        int lineStart = line->start_index;
        int lineEnd = lineStart + line->length;
        int lo = u8a > lineStart ? u8a : lineStart;
        int hi = u8b < lineEnd ? u8b : lineEnd;
        if (lo >= hi) {
            continue;
        }
        int x0 = 0, x1 = 0;
        pango_layout_line_index_to_x(line, lo, FALSE, &x0);
        pango_layout_line_index_to_x(line, hi, FALSE, &x1);
        int y0 = 0, y1 = 0;
        pango_layout_iter_get_line_yrange(iter, &y0, &y1);
        float left = (float)x0 / PANGO_SCALE;
        float right = (float)x1 / PANGO_SCALE;
        if (right < left) {
            float t = left;
            left = right;
            right = t;
        }
        out[n].x = left;
        out[n].y = (float)y0 / PANGO_SCALE + pad;
        out[n].w = right - left;
        out[n].h = (float)(y1 - y0) / PANGO_SCALE;
        n++;
    } while (n < max && pango_layout_iter_next_line(iter));
    pango_layout_iter_free(iter);
    return n;
}

} // namespace gpui
