#include "gpui/svg.h"
#include "gpui/asset_icons.h"
#include "gpui/assets.h"
#include "gpui/drawops.h"
#include "gpui/paint.h"

#include <math.h>

namespace gpui {

// Reading an `.svg` and drawing one are two different jobs now. This file does
// the first: a Lucide-style file - viewBox, path/rect/polyline/line/circle/
// polygon - becomes the byte stream `drawops.h` describes, and
// `ExecuteDrawOps` is what paints it. `assets/icons` never comes through here
// at runtime: `cmd/svg-to-bytecode.ts` ran this same conversion ahead of time
// and its output is `asset_icons.cpp`, so the two have to keep agreeing.

enum SvgCmd : uint8_t {
    kMove = 0,
    kLine = 1,
    kCubic = 2,
    kClose = 3
};

struct SvgOp {
    uint8_t cmd = kMove;
    float x = 0, y = 0;
    float x1 = 0, y1 = 0;
    float x2 = 0, y2 = 0;
};

// One drawn element of the file - a <path>, a <rect>, a <circle> - and the run
// of ops it contributed. A Lucide icon says nothing about colour and every
// shape takes the caller's; a picture with colours of its own names them per
// shape, which is what a two-tone logo is.
struct SvgShape {
    int start = 0;
    int count = 0;
    bool hasFill = false;
    Rgba fill = {};
};

// The file, read but not yet encoded. It lives for the length of one
// conversion; the bytes it turns into are what is kept.
struct SvgIcon {
    float vbX = 0, vbY = 0, vbW = 24, vbH = 24;
    float strokeW = 2;
    // fill="currentColor" on the root: the solid variants (star-fill, ...) are
    // filled and stroked, everything else is stroke only.
    bool filled = false;
    // Whether any shape named a colour. False is every Lucide icon, and the
    // whole file is then one path in the caller's colour, as it always was.
    bool hasOwnColors = false;
    Vec<SvgOp> ops;
    Vec<SvgShape> shapes;
};

static void AddOp(SvgIcon* ic, SvgOp op) {
    ic->ops.Append(op);
}

static void AddMove(SvgIcon* ic, float x, float y) {
    SvgOp o;
    o.cmd = kMove;
    o.x = x;
    o.y = y;
    AddOp(ic, o);
}
static void AddLine(SvgIcon* ic, float x, float y) {
    SvgOp o;
    o.cmd = kLine;
    o.x = x;
    o.y = y;
    AddOp(ic, o);
}
static void AddCubic(SvgIcon* ic, float x1, float y1, float x2, float y2,
                     float x, float y) {
    SvgOp o;
    o.cmd = kCubic;
    o.x1 = x1;
    o.y1 = y1;
    o.x2 = x2;
    o.y2 = y2;
    o.x = x;
    o.y = y;
    AddOp(ic, o);
}
static void AddClose(SvgIcon* ic) {
    SvgOp o;
    o.cmd = kClose;
    AddOp(ic, o);
}

static void AddRoundRect(SvgIcon* ic, float x, float y, float w, float h,
                         float rx) {
    if (rx < 0) {
        rx = 0;
    }
    if (rx > w * 0.5f) {
        rx = w * 0.5f;
    }
    if (rx > h * 0.5f) {
        rx = h * 0.5f;
    }
    if (rx <= 0.01f) {
        AddMove(ic, x, y);
        AddLine(ic, x + w, y);
        AddLine(ic, x + w, y + h);
        AddLine(ic, x, y + h);
        AddClose(ic);
        return;
    }
    // Cubic kappa for quarter circle
    float k = rx * 0.55228475f;
    float x1 = x + rx, x2 = x + w - rx;
    float y1 = y + rx, y2 = y + h - rx;
    AddMove(ic, x1, y);
    AddLine(ic, x2, y);
    AddCubic(ic, x2 + k, y, x + w, y1 - k, x + w, y1);
    AddLine(ic, x + w, y2);
    AddCubic(ic, x + w, y2 + k, x2 + k, y + h, x2, y + h);
    AddLine(ic, x1, y + h);
    AddCubic(ic, x1 - k, y + h, x, y2 + k, x, y2);
    AddLine(ic, x, y1);
    AddCubic(ic, x, y1 - k, x1 - k, y, x1, y);
    AddClose(ic);
}

// ─── path d parser ────────────────────────────────────────────────────────

struct PathScan {
    const char* p;
    const char* end;
};

static void SkipWs(PathScan* s) {
    while (s->p < s->end && (*s->p == ' ' || *s->p == '\t' || *s->p == '\n' ||
                             *s->p == '\r' || *s->p == ',')) {
        s->p++;
    }
}

static bool ParseNum(PathScan* s, float* out) {
    SkipWs(s);
    if (s->p >= s->end) {
        return false;
    }
    char* endp = nullptr;
    float v = strtof(s->p, &endp);
    if (endp == s->p) {
        return false;
    }
    *out = v;
    s->p = endp;
    return true;
}

static float Angle(float ux, float uy, float vx, float vy) {
    float dot = ux * vx + uy * vy;
    float nu = sqrtf(ux * ux + uy * uy);
    float nv = sqrtf(vx * vx + vy * vy);
    float c = (nu > 0 && nv > 0) ? dot / (nu * nv) : 1;
    if (c < -1) {
        c = -1;
    }
    if (c > 1) {
        c = 1;
    }
    float a = acosf(c);
    if (ux * vy - uy * vx < 0) {
        a = -a;
    }
    return a;
}

static void AddArc(SvgIcon* ic, float x1, float y1, float rx, float ry,
                   float phiDeg, bool large, bool sweep, float x2, float y2) {
    rx = fabsf(rx);
    ry = fabsf(ry);
    if (rx < 1e-6f || ry < 1e-6f) {
        AddLine(ic, x2, y2);
        return;
    }
    float phi = phiDeg * kPi / 180.f;
    float cosP = cosf(phi);
    float sinP = sinf(phi);
    float dx = (x1 - x2) * 0.5f;
    float dy = (y1 - y2) * 0.5f;
    float x1p = cosP * dx + sinP * dy;
    float y1p = -sinP * dx + cosP * dy;
    float rx2 = rx * rx, ry2 = ry * ry;
    float x1p2 = x1p * x1p, y1p2 = y1p * y1p;
    float lam = x1p2 / rx2 + y1p2 / ry2;
    if (lam > 1) {
        float sc = sqrtf(lam);
        rx *= sc;
        ry *= sc;
        rx2 = rx * rx;
        ry2 = ry * ry;
    }
    float num = rx2 * ry2 - rx2 * y1p2 - ry2 * x1p2;
    float den = rx2 * y1p2 + ry2 * x1p2;
    float csq = (den > 0) ? num / den : 0;
    if (csq < 0) {
        csq = 0;
    }
    float c = sqrtf(csq);
    if (large == sweep) {
        c = -c;
    }
    float cxp = c * rx * y1p / ry;
    float cyp = c * -ry * x1p / rx;
    float cx = cosP * cxp - sinP * cyp + (x1 + x2) * 0.5f;
    float cy = sinP * cxp + cosP * cyp + (y1 + y2) * 0.5f;
    float theta1 = Angle(1, 0, (x1p - cxp) / rx, (y1p - cyp) / ry);
    float dtheta = Angle((x1p - cxp) / rx, (y1p - cyp) / ry, (-x1p - cxp) / rx,
                         (-y1p - cyp) / ry);
    if (!sweep && dtheta > 0) {
        dtheta -= 2 * kPi;
    }
    if (sweep && dtheta < 0) {
        dtheta += 2 * kPi;
    }
    int segs = (int)ceilf(fabsf(dtheta) / (kPi * 0.5f + 1e-6f));
    if (segs < 1) {
        segs = 1;
    }
    if (segs > 8) {
        segs = 8;
    }
    float dt = dtheta / (float)segs;
    for (int i = 0; i < segs; i++) {
        float t0 = theta1 + dt * (float)i;
        float t1 = t0 + dt;
        float e0x = rx * cosf(t0), e0y = ry * sinf(t0);
        float e1x = rx * cosf(t1), e1y = ry * sinf(t1);
        float q = tanf(dt * 0.5f);
        float alpha = sinf(dt) * (sqrtf(4 + 3 * q * q) - 1) / 3.f;
        float d0x = -rx * sinf(t0), d0y = ry * cosf(t0);
        float d1x = -rx * sinf(t1), d1y = ry * cosf(t1);
        float p0x = cx + cosP * e0x - sinP * e0y;
        float p0y = cy + sinP * e0x + cosP * e0y;
        (void)p0x;
        (void)p0y;
        float p1x = cx + cosP * e1x - sinP * e1y;
        float p1y = cy + sinP * e1x + cosP * e1y;
        float c1x =
            cx + cosP * (e0x + alpha * d0x) - sinP * (e0y + alpha * d0y);
        float c1y =
            cy + sinP * (e0x + alpha * d0x) + cosP * (e0y + alpha * d0y);
        float c2x =
            cx + cosP * (e1x - alpha * d1x) - sinP * (e1y - alpha * d1y);
        float c2y =
            cy + sinP * (e1x - alpha * d1x) + cosP * (e1y - alpha * d1y);
        AddCubic(ic, c1x, c1y, c2x, c2y, p1x, p1y);
    }
}

static void ParsePathD(SvgIcon* ic, Str d) {
    if (!d.s || d.len <= 0) {
        return;
    }
    PathScan s{d.s, d.s + d.len};
    char cmd = 0;
    float cx = 0, cy = 0, sx = 0, sy = 0;
    float pcx = 0, pcy = 0; // previous cubic control (for S)
    bool hasPrevC = false;
    while (s.p < s.end) {
        SkipWs(&s);
        if (s.p >= s.end) {
            break;
        }
        char c = *s.p;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            cmd = c;
            s.p++;
        } else if (!cmd) {
            s.p++;
            continue;
        }
        bool rel = cmd >= 'a';
        char op = rel ? (char)(cmd - 32) : cmd;
        if (op == 'Z') {
            AddClose(ic);
            cx = sx;
            cy = sy;
            hasPrevC = false;
            continue;
        }
        if (op == 'M') {
            float x, y;
            if (!ParseNum(&s, &x) || !ParseNum(&s, &y)) {
                break;
            }
            if (rel) {
                x += cx;
                y += cy;
            }
            AddMove(ic, x, y);
            cx = sx = x;
            cy = sy = y;
            hasPrevC = false;
            // extra pairs are implicit L/l
            cmd = rel ? 'l' : 'L';
            continue;
        }
        if (op == 'L') {
            float x, y;
            if (!ParseNum(&s, &x) || !ParseNum(&s, &y)) {
                break;
            }
            if (rel) {
                x += cx;
                y += cy;
            }
            AddLine(ic, x, y);
            cx = x;
            cy = y;
            hasPrevC = false;
            continue;
        }
        if (op == 'H') {
            float x;
            if (!ParseNum(&s, &x)) {
                break;
            }
            if (rel) {
                x += cx;
            }
            AddLine(ic, x, cy);
            cx = x;
            hasPrevC = false;
            continue;
        }
        if (op == 'V') {
            float y;
            if (!ParseNum(&s, &y)) {
                break;
            }
            if (rel) {
                y += cy;
            }
            AddLine(ic, cx, y);
            cy = y;
            hasPrevC = false;
            continue;
        }
        if (op == 'C') {
            float x1, y1, x2, y2, x, y;
            if (!ParseNum(&s, &x1) || !ParseNum(&s, &y1) ||
                !ParseNum(&s, &x2) || !ParseNum(&s, &y2) || !ParseNum(&s, &x) ||
                !ParseNum(&s, &y)) {
                break;
            }
            if (rel) {
                x1 += cx;
                y1 += cy;
                x2 += cx;
                y2 += cy;
                x += cx;
                y += cy;
            }
            AddCubic(ic, x1, y1, x2, y2, x, y);
            pcx = x2;
            pcy = y2;
            hasPrevC = true;
            cx = x;
            cy = y;
            continue;
        }
        if (op == 'S') {
            float x2, y2, x, y;
            if (!ParseNum(&s, &x2) || !ParseNum(&s, &y2) || !ParseNum(&s, &x) ||
                !ParseNum(&s, &y)) {
                break;
            }
            if (rel) {
                x2 += cx;
                y2 += cy;
                x += cx;
                y += cy;
            }
            float x1 = hasPrevC ? (2 * cx - pcx) : cx;
            float y1 = hasPrevC ? (2 * cy - pcy) : cy;
            AddCubic(ic, x1, y1, x2, y2, x, y);
            pcx = x2;
            pcy = y2;
            hasPrevC = true;
            cx = x;
            cy = y;
            continue;
        }
        if (op == 'Q') {
            float x1, y1, x, y;
            if (!ParseNum(&s, &x1) || !ParseNum(&s, &y1) || !ParseNum(&s, &x) ||
                !ParseNum(&s, &y)) {
                break;
            }
            if (rel) {
                x1 += cx;
                y1 += cy;
                x += cx;
                y += cy;
            }
            // elevate quad to cubic
            float c1x = cx + 2.f / 3.f * (x1 - cx);
            float c1y = cy + 2.f / 3.f * (y1 - cy);
            float c2x = x + 2.f / 3.f * (x1 - x);
            float c2y = y + 2.f / 3.f * (y1 - y);
            AddCubic(ic, c1x, c1y, c2x, c2y, x, y);
            pcx = x1;
            pcy = y1;
            hasPrevC = true;
            cx = x;
            cy = y;
            continue;
        }
        if (op == 'T') {
            float x, y;
            if (!ParseNum(&s, &x) || !ParseNum(&s, &y)) {
                break;
            }
            if (rel) {
                x += cx;
                y += cy;
            }
            float x1 = hasPrevC ? (2 * cx - pcx) : cx;
            float y1 = hasPrevC ? (2 * cy - pcy) : cy;
            float c1x = cx + 2.f / 3.f * (x1 - cx);
            float c1y = cy + 2.f / 3.f * (y1 - cy);
            float c2x = x + 2.f / 3.f * (x1 - x);
            float c2y = y + 2.f / 3.f * (y1 - y);
            AddCubic(ic, c1x, c1y, c2x, c2y, x, y);
            pcx = x1;
            pcy = y1;
            hasPrevC = true;
            cx = x;
            cy = y;
            continue;
        }
        if (op == 'A') {
            float rx, ry, rot, x, y;
            float fA, fS;
            if (!ParseNum(&s, &rx) || !ParseNum(&s, &ry) ||
                !ParseNum(&s, &rot) || !ParseNum(&s, &fA) ||
                !ParseNum(&s, &fS) || !ParseNum(&s, &x) || !ParseNum(&s, &y)) {
                break;
            }
            if (rel) {
                x += cx;
                y += cy;
            }
            AddArc(ic, cx, cy, rx, ry, rot, fA != 0, fS != 0, x, y);
            cx = x;
            cy = y;
            hasPrevC = false;
            continue;
        }
        // unknown command
        s.p++;
    }
}

static void ParsePolyline(SvgIcon* ic, Str pts, bool close) {
    PathScan s{pts.s, pts.s + pts.len};
    bool first = true;
    float x, y;
    while (ParseNum(&s, &x) && ParseNum(&s, &y)) {
        if (first) {
            AddMove(ic, x, y);
            first = false;
        } else {
            AddLine(ic, x, y);
        }
    }
    if (close && !first) {
        AddClose(ic);
    }
}

// ─── tiny SVG tag scanner ─────────────────────────────────────────────────

static bool StartsWithI(const char* p, const char* end, const char* lit) {
    int n = (int)strlen(lit);
    if (p + n > end) {
        return false;
    }
    for (int i = 0; i < n; i++) {
        char a = p[i], b = lit[i];
        if (a >= 'A' && a <= 'Z') {
            a = (char)(a + 32);
        }
        if (a != b) {
            return false;
        }
    }
    return true;
}

static bool IsIdentChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '-' || c == '_';
}

// `name="value"` inside one tag's text, as a slice of the tag. Matched only
// on a name boundary, so "fill" does not match "fill-rule". A slice rather
// than a copy because a `d` attribute has no bound worth guessing at — a
// window-chrome icon traced by a design tool runs past two thousand
// characters, and a fixed buffer silently drew half of it.
static bool GetAttrStr(Str tag, const char* name, Str* out) {
    int nlen = (int)strlen(name);
    const char* p = tag.s;
    const char* end = tag.s + tag.len;
    while (p + nlen + 2 < end) {
        bool bound = (p == tag.s) || !IsIdentChar(p[-1]);
        if (bound && StrCmpNI(p, name, nlen) == 0 && p[nlen] == '=') {
            p += nlen + 1;
            char q = 0;
            if (*p == '"' || *p == '\'') {
                q = *p++;
            }
            const char* vs = p;
            while (p < end && *p != q && *p != '>') {
                p++;
            }
            *out = Str(vs, (int)(p - vs));
            return out->len > 0;
        }
        p++;
    }
    return false;
}

// The same, copied and null-terminated, for the short ones an `atof` or a
// colour parse wants as a C string.
static bool GetAttr(Str tag, const char* name, char* out, int outN) {
    Str v;
    if (!GetAttrStr(tag, name, &v)) {
        out[0] = 0;
        return false;
    }
    int n = v.len < outN - 1 ? v.len : outN - 1;
    memcpy(out, v.s, (size_t)n);
    out[n] = 0;
    return n > 0;
}

static float AttrF(Str tag, const char* name, float def) {
    char buf[64];
    if (!GetAttr(tag, name, buf, 64)) {
        return def;
    }
    return (float)atof(buf);
}

// `fill="#rrggbb"` on a shape. "none" and "currentColor" both leave the shape
// in the caller's colour, which is what every Lucide icon says.
static bool ParseSvgColor(Str s, Rgba* out) {
    if (!s.s || s.len < 4 || s.s[0] != '#') {
        return false;
    }
    int n = s.len - 1;
    if (n != 3 && n != 6) {
        return false;
    }
    int v[6] = {};
    for (int i = 0; i < n; i++) {
        char c = s.s[i + 1];
        if (c >= '0' && c <= '9') {
            v[i] = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            v[i] = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            v[i] = c - 'A' + 10;
        } else {
            return false;
        }
    }
    if (n == 3) {
        *out = Rgba{(uint8_t)(v[0] * 17), (uint8_t)(v[1] * 17),
                    (uint8_t)(v[2] * 17), 255};
    } else {
        *out = Rgba{(uint8_t)(v[0] * 16 + v[1]), (uint8_t)(v[2] * 16 + v[3]),
                    (uint8_t)(v[4] * 16 + v[5]), 255};
    }
    return true;
}

// One drawn element is done: what it added, and the colour it named. Every
// element gets one, `<line>` and `<polyline>` included — a shape that is not
// on the list is a shape the per-colour encoding would drop.
static void EndShape(SvgIcon* ic, int start, Str tag) {
    if (ic->ops.len <= start) {
        return;
    }
    SvgShape sh;
    sh.start = start;
    sh.count = ic->ops.len - start;
    char fill[64];
    if (GetAttr(tag, "fill", fill, 64) && ParseSvgColor(Str(fill), &sh.fill)) {
        sh.hasFill = true;
        ic->hasOwnColors = true;
    }
    ic->shapes.Append(sh);
}

static void ParseSvg(Str xml, SvgIcon* ic) {
    ic->ops.Reset();
    ic->shapes.Reset();
    ic->vbX = 0;
    ic->vbY = 0;
    ic->vbW = 24;
    ic->vbH = 24;
    ic->strokeW = 2;
    ic->filled = false;
    ic->hasOwnColors = false;
    if (!xml.s || xml.len <= 0) {
        return;
    }
    const char* p = xml.s;
    const char* end = xml.s + xml.len;
    while (p < end) {
        if (*p != '<') {
            p++;
            continue;
        }
        p++;
        if (p < end && *p == '/') {
            while (p < end && *p != '>') {
                p++;
            }
            if (p < end) {
                p++;
            }
            continue;
        }
        if (p < end && *p == '!') {
            // comment / doctype
            while (p + 2 < end &&
                   !(p[0] == '-' && p[1] == '-' && p[2] == '>')) {
                p++;
            }
            p += 3;
            continue;
        }
        const char* tagStart = p;
        while (p < end && *p != '>') {
            p++;
        }
        if (p >= end) {
            break;
        }
        Str tag(tagStart, (int)(p - tagStart));
        p++; // skip >

        if (StartsWithI(tagStart, end, "svg")) {
            char vb[64];
            if (GetAttr(tag, "viewBox", vb, 64)) {
                PathScan s{vb, vb + strlen(vb)};
                float a = 0, b = 0, c = 24, d = 24;
                ParseNum(&s, &a);
                ParseNum(&s, &b);
                ParseNum(&s, &c);
                ParseNum(&s, &d);
                ic->vbX = a;
                ic->vbY = b;
                ic->vbW = c > 0 ? c : 24;
                ic->vbH = d > 0 ? d : 24;
            }
            float sw = AttrF(tag, "stroke-width", 0);
            if (sw > 0) {
                ic->strokeW = sw;
            }
            char fill[64];
            if (GetAttr(tag, "fill", fill, 64)) {
                ic->filled = !StrEqI(Str(fill), StrL("none"));
            }
            continue;
        }
        if (StartsWithI(tagStart, end, "path")) {
            Str d;
            int start = ic->ops.len;
            if (GetAttrStr(tag, "d", &d)) {
                ParsePathD(ic, d);
            }
            EndShape(ic, start, tag);
            continue;
        }
        if (StartsWithI(tagStart, end, "rect")) {
            int start = ic->ops.len;
            float x = AttrF(tag, "x", 0);
            float y = AttrF(tag, "y", 0);
            float w = AttrF(tag, "width", 0);
            float h = AttrF(tag, "height", 0);
            float rx = AttrF(tag, "rx", 0);
            AddRoundRect(ic, x, y, w, h, rx);
            EndShape(ic, start, tag);
            continue;
        }
        if (StartsWithI(tagStart, end, "polyline")) {
            Str pts;
            int start = ic->ops.len;
            if (GetAttrStr(tag, "points", &pts)) {
                ParsePolyline(ic, pts, false);
            }
            EndShape(ic, start, tag);
            continue;
        }
        if (StartsWithI(tagStart, end, "polygon")) {
            Str pts;
            int start = ic->ops.len;
            if (GetAttrStr(tag, "points", &pts)) {
                ParsePolyline(ic, pts, true);
            }
            EndShape(ic, start, tag);
            continue;
        }
        if (StartsWithI(tagStart, end, "line")) {
            int start = ic->ops.len;
            float x1 = AttrF(tag, "x1", 0);
            float y1 = AttrF(tag, "y1", 0);
            float x2 = AttrF(tag, "x2", 0);
            float y2 = AttrF(tag, "y2", 0);
            AddMove(ic, x1, y1);
            AddLine(ic, x2, y2);
            EndShape(ic, start, tag);
            continue;
        }
        if (StartsWithI(tagStart, end, "circle")) {
            int start = ic->ops.len;
            float cx = AttrF(tag, "cx", 0);
            float cy = AttrF(tag, "cy", 0);
            float r = AttrF(tag, "r", 0);
            AddRoundRect(ic, cx - r, cy - r, r * 2, r * 2, r);
            EndShape(ic, start, tag);
            continue;
        }
    }
}

// ─── the file, as bytecode ────────────────────────────────

static void EmitOps(DrawOpsBuilder* b, const SvgIcon* ic, int from, int to) {
    for (int i = from; i < to; i++) {
        const SvgOp& o = ic->ops[i];
        if (o.cmd == kMove) {
            b->MoveTo(o.x, o.y);
        } else if (o.cmd == kLine) {
            b->LineTo(o.x, o.y);
        } else if (o.cmd == kCubic) {
            b->CubicTo(o.x1, o.y1, o.x2, o.y2, o.x, o.y);
        } else if (o.cmd == kClose) {
            b->ClosePath();
        }
    }
}

// The rule `cmd/svg-to-bytecode.ts` implements too. A plain Lucide icon is one
// path in the caller's colour - filled first if the root said
// fill="currentColor", then stroked. A file whose shapes name their own
// colours is a picture, not an icon: each shape is painted on its own, so it
// keeps the colour it asked for and the ones that named none still take the
// caller's.
static void EncodeIcon(const SvgIcon* ic, DrawOpsBuilder* b) {
    b->ViewBox(ic->vbX, ic->vbY, ic->vbW, ic->vbH);
    b->StrokeWidth(ic->strokeW > 0 ? ic->strokeW : 2.f);
    if (!ic->hasOwnColors) {
        EmitOps(b, ic, 0, ic->ops.len);
        b->Op(ic->filled ? kOpFillStrokePath : kOpStrokePath);
        b->End();
        return;
    }
    for (int i = 0; i < ic->shapes.len; i++) {
        const SvgShape& sh = ic->shapes[i];
        if (sh.hasFill) {
            b->Color(sh.fill);
        }
        EmitOps(b, ic, sh.start, sh.start + sh.count);
        if (sh.hasFill) {
            b->Op(kOpFillPath);
            b->ColorReset();
        } else if (ic->filled) {
            b->Op(kOpFillPath);
        } else {
            b->Op(kOpStrokePath);
        }
    }
    b->End();
}

bool SvgToDrawOps(Str xml, DrawOpsBuilder* out) {
    if (!out) {
        return false;
    }
    SvgIcon ic;
    ParseSvg(xml, &ic);
    if (ic.ops.len <= 0) {
        return false;
    }
    EncodeIcon(&ic, out);
    return true;
}

// ─── what an asset path draws ──────────────────────────────

// The generated table, looked up. `kAssetIcons` is sorted by name, so this is
// a binary search over 70-odd entries rather than a walk.
const uint8_t* AssetIconFind(Str name, int* lenOut) {
    *lenOut = 0;
    if (!name.s || name.len <= 0) {
        return nullptr;
    }
    int lo = 0;
    int hi = kAssetIconsCount - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        const AssetIcon& e = kAssetIcons[mid];
        int cmp = StrCmpNI(e.name, name.s, name.len);
        if (cmp == 0 && e.name[name.len] != 0) {
            cmp = 1; // a longer name sorts after the prefix asked for
        }
        if (cmp == 0) {
            *lenOut = e.len;
            return kAssetIconsData + e.offset;
        }
        if (cmp < 0) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return nullptr;
}

const uint8_t* AssetIconForPath(Str assetPath, int* lenOut) {
    *lenOut = 0;
    const char* kDir = "icons/";
    const int kDirLen = 6;
    const int kExtLen = 4; // ".svg"
    if (assetPath.len <= kDirLen + kExtLen) {
        return nullptr;
    }
    if (StrCmpNI(assetPath.s, kDir, kDirLen) != 0) {
        return nullptr;
    }
    Str base(assetPath.s + kDirLen, assetPath.len - kDirLen - kExtLen);
    if (StrCmpNI(base.s + base.len, ".svg", kExtLen) != 0) {
        return nullptr;
    }
    return AssetIconFind(base, lenOut);
}

// What an asset path resolved to, looked up once and then remembered. An
// application's own `.svg` is read and converted here; a path that no root
// supplies falls through to the compiled table, and that answer is cached
// too — otherwise every icon would stat the filesystem once a frame.
//
// Big enough to hold the whole icon set and then some. It wraps rather than
// grows, and a wrap frees whatever the slot was holding.
static const int kMaxCache = 128;

struct OpsCache {
    char path[128] = {};
    const uint8_t* data = nullptr;
    int len = 0;
    // False when `data` points into kAssetIconsData, which is not ours to
    // free.
    bool owned = false;
};

static OpsCache gCache[kMaxCache];
static int gCacheN = 0;

// The slot `assetPath` will live in, emptied and named. The caller has
// already bounded the path, so this always has one to give.
static OpsCache* CacheSlotFor(Str assetPath) {
    if (gCacheN >= kMaxCache) {
        gCacheN = 0; // simple wrap
    }
    OpsCache* e = &gCache[gCacheN++];
    if (e->owned) {
        Free(nullptr, (void*)e->data);
    }
    e->data = nullptr;
    e->len = 0;
    e->owned = false;
    memcpy(e->path, assetPath.s, (size_t)assetPath.len);
    e->path[assetPath.len] = 0;
    return e;
}

const uint8_t* SvgDrawOpsFor(Str assetPath, int* lenOut) {
    *lenOut = 0;
    if (!assetPath.s || assetPath.len <= 0 || assetPath.len > 127) {
        return nullptr;
    }
    for (int i = 0; i < gCacheN; i++) {
        if (gCache[i].data &&
            StrCmpNI(gCache[i].path, assetPath.s, assetPath.len) == 0 &&
            gCache[i].path[assetPath.len] == 0) {
            *lenOut = gCache[i].len;
            return gCache[i].data;
        }
    }
    // The asset roots first: an application that ships "icons/inbox.svg" of
    // its own means that one, the way Rust's AssetSource does. Only when no
    // root has the file does the compiled-in table answer — which is every
    // lucide icon in a binary shipped without its assets folder beside it.
    TempStr xml = AssetsLoadTextTemp(assetPath);
    if (xml.s) {
        DrawOpsBuilder b;
        if (SvgToDrawOps(xml, &b)) {
            uint8_t* buf = AllocArray<uint8_t>(b.data.len);
            if (!buf) {
                return nullptr;
            }
            memcpy(buf, b.data.els, (size_t)b.data.len);
            OpsCache* e = CacheSlotFor(assetPath);
            e->data = buf;
            e->len = b.data.len;
            e->owned = true;
            *lenOut = e->len;
            return e->data;
        }
    }
    int len = 0;
    const uint8_t* built = AssetIconForPath(assetPath, &len);
    if (!built) {
        return nullptr;
    }
    // Remembered as well, so a name the table answers is not a directory
    // walk every time it is drawn.
    OpsCache* e = CacheSlotFor(assetPath);
    e->data = built;
    e->len = len;
    *lenOut = len;
    return built;
}

bool SvgViewBox(Str assetPath, Size* out) {
    int len = 0;
    const uint8_t* ops = SvgDrawOpsFor(assetPath, &len);
    if (!ops || !out) {
        return false;
    }
    return DrawOpsViewBox(ops, len, out);
}

bool SvgDraw(PaintCtx* ctx, Str assetPath, float x, float y, float size,
             Rgba color, float turns) {
    if (!ctx || !ctx->rt || size <= 0) {
        return false;
    }
    int len = 0;
    const uint8_t* ops = SvgDrawOpsFor(assetPath, &len);
    if (!ops) {
        return false;
    }
    DrawOpsTarget t;
    t.x = x;
    t.y = y;
    t.w = size;
    t.h = size;
    t.color = color;
    t.turns = turns;
    return ExecuteDrawOps(ctx, ops, len, t);
}

Str IconNamePath(IconName name) {
    switch (name) {
        case IconName::ArrowLeft:
            return StrL("icons/arrow-left.svg");
        case IconName::ArrowRight:
            return StrL("icons/arrow-right.svg");
        case IconName::Asterisk:
            return StrL("icons/asterisk.svg");
        case IconName::Bell:
            return StrL("icons/bell.svg");
        case IconName::Building2:
            return StrL("icons/building-2.svg");
        case IconName::Eye:
            return StrL("icons/eye.svg");
        case IconName::EyeOff:
            return StrL("icons/eye-off.svg");
        case IconName::Heart:
            return StrL("icons/heart.svg");
        case IconName::HeartOff:
            return StrL("icons/heart-off.svg");
        case IconName::Maximize:
            return StrL("icons/maximize.svg");
        case IconName::Minimize:
            return StrL("icons/minimize.svg");
        case IconName::Star:
            return StrL("icons/star.svg");
        case IconName::StarFill:
            return StrL("icons/star-fill.svg");
        case IconName::Sun:
            return StrL("icons/sun.svg");
        case IconName::Moon:
            return StrL("icons/moon.svg");
        case IconName::Play:
            return StrL("icons/play.svg");
        case IconName::Map:
            return StrL("icons/map.svg");
        case IconName::Globe:
            return StrL("icons/globe.svg");
        case IconName::Github:
            return StrL("icons/github.svg");
        case IconName::ExternalLink:
            return StrL("icons/external-link.svg");
        case IconName::Inbox:
            return StrL("icons/inbox.svg");
        case IconName::Bot:
            return StrL("icons/bot.svg");
        case IconName::Cpu:
            return StrL("icons/cpu.svg");
        case IconName::MemoryStick:
            return StrL("icons/memory-stick.svg");
        case IconName::HardDrive:
            return StrL("icons/hard-drive.svg");
        case IconName::Battery:
            return StrL("icons/battery.svg");
        case IconName::BatteryCharging:
            return StrL("icons/battery-charging.svg");
        case IconName::BatteryMedium:
            return StrL("icons/battery-medium.svg");
        case IconName::BatteryFull:
            return StrL("icons/battery-full.svg");
        case IconName::WindowMinimize:
            return StrL("icons/window-minimize.svg");
        case IconName::WindowMaximize:
            return StrL("icons/window-maximize.svg");
        case IconName::WindowRestore:
            return StrL("icons/window-restore.svg");
        case IconName::WindowClose:
            return StrL("icons/window-close.svg");
        case IconName::LayoutDashboard:
            return StrL("icons/layout-dashboard.svg");
        case IconName::Calendar:
            return StrL("icons/calendar.svg");
        case IconName::Folder:
            return StrL("icons/folder.svg");
        case IconName::Settings:
            return StrL("icons/settings.svg");
        case IconName::GalleryVerticalEnd:
            return StrL("icons/gallery-vertical-end.svg");
        case IconName::CircleUser:
            return StrL("icons/circle-user.svg");
        case IconName::User:
            return StrL("icons/user.svg");
        case IconName::PanelLeft:
            return StrL("icons/panel-left.svg");
        case IconName::PanelLeftOpen:
            return StrL("icons/panel-left-open.svg");
        case IconName::PanelLeftClose:
            return StrL("icons/panel-left-close.svg");
        case IconName::PanelRightOpen:
            return StrL("icons/panel-right-open.svg");
        case IconName::PanelRightClose:
            return StrL("icons/panel-right-close.svg");
        case IconName::Info:
            return StrL("icons/info.svg");
        case IconName::X:
            return StrL("icons/x.svg");
        case IconName::CircleCheck:
            return StrL("icons/circle-check.svg");
        case IconName::TriangleAlert:
            return StrL("icons/triangle-alert.svg");
        case IconName::CircleX:
            return StrL("icons/circle-x.svg");
        case IconName::Loader:
            return StrL("icons/loader.svg");
        case IconName::LoaderCircle:
            return StrL("icons/loader-circle.svg");
        case IconName::Ellipsis:
            return StrL("icons/ellipsis.svg");
        case IconName::ChevronsUpDown:
            return StrL("icons/chevrons-up-down.svg");
        case IconName::SquareTerminal:
            return StrL("icons/square-terminal.svg");
        case IconName::BookOpen:
            return StrL("icons/book-open.svg");
        case IconName::Settings2:
            return StrL("icons/settings-2.svg");
        case IconName::Frame:
            return StrL("icons/frame.svg");
        case IconName::ChartPie:
            return StrL("icons/chart-pie.svg");
        case IconName::Palette:
            return StrL("icons/palette.svg");
        case IconName::File:
            return StrL("icons/file.svg");
        case IconName::FolderOpen:
            return StrL("icons/folder-open.svg");
        case IconName::ChevronDown:
            return StrL("icons/chevron-down.svg");
        case IconName::ChevronLeft:
            return StrL("icons/chevron-left.svg");
        case IconName::ChevronRight:
            return StrL("icons/chevron-right.svg");
        case IconName::CaseSensitive:
            return StrL("icons/case-sensitive.svg");
        case IconName::Replace:
            return StrL("icons/replace.svg");
        case IconName::ChevronUp:
            return StrL("icons/chevron-up.svg");
        case IconName::Check:
            return StrL("icons/check.svg");
        case IconName::Search:
            return StrL("icons/search.svg");
        case IconName::Minus:
            return StrL("icons/minus.svg");
        case IconName::Plus:
            return StrL("icons/plus.svg");
        case IconName::Copy:
            return StrL("icons/copy.svg");
        default:
            return {};
    }
}

bool SvgRasterize(PaintApp* pa, Str assetPath, int px, Rgba color,
                  uint8_t* outBgra) {
    if (!pa || px <= 0 || !outBgra) {
        return false;
    }
    PaintCtx ctx = {};
    ctx.pa = pa;
    ctx.dpi = 96;
    ctx.viewW = (float)px;
    ctx.viewH = (float)px;
    if (!PaintTargetBeginOffscreen(&ctx, px, px)) {
        return false;
    }
    bool drew = SvgDraw(&ctx, assetPath, 0, 0, (float)px, color);
    bool ok = PaintTargetEndOffscreen(&ctx, outBgra);
    return drew && ok;
}

} // namespace gpui
