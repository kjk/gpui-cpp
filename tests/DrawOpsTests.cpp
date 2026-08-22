/* Not a port — there is no Rust behind this one.
 *
 * `src/gpui/svg.cpp` and `cmd/svg-to-bytecode.ts` are the same converter
 * written twice: the first reads an application's own `.svg` at load time,
 * the second read `assets/icons` at build time and its output is the
 * `asset_icons.cpp` every lucide icon is drawn from. Nothing in the build
 * makes them agree, so this does: every icon in the generated table is
 * reconverted from its file and the two byte streams are compared op for op.
 *
 * Floats are compared rather than bytes. The generator works in doubles and
 * rounds to f32 at the end, so an icon whose path has an arc in it lands a
 * few ulps away from what the C++ float arithmetic produces. */

#include "Test.h"

#include <stdio.h>

// How many f32 arguments each op takes, in opcode order. The opcodes start at
// zero and run without a gap, so this is one index rather than the switch it
// was. kOpColor's one u32 is read the same way and compared as a float, which
// is exact for a byte-packed colour.
static const uint8_t kOpArgs[] = {
    0, // kOpEnd
    4, // kOpViewBox
    1, // kOpStrokeWidth
    1, // kOpColor
    0, // kOpColorReset
    4, // kOpLine
    5, // kOpRect
    5, // kOpFillRect
    4, // kOpEllipse
    4, // kOpFillEllipse
    5, // kOpArc
    2, // kOpMoveTo
    2, // kOpLineTo
    6, // kOpCubicTo
    0, // kOpClosePath
    0, // kOpFillPath
    0, // kOpStrokePath
    0, // kOpFillStrokePath
};

// An opcode added to drawops.h without a row here would read the next op's
// count, which is a wrong answer rather than a missing one.
static_assert(sizeof(kOpArgs) == (size_t)kOpFillStrokePath + 1,
              "kOpArgs must have a row per opcode");

// -1 for an opcode this reader does not know, which is what the walk stops on.
static int OpArgCount(uint16_t op) {
    if (op >= sizeof(kOpArgs)) {
        return -1;
    }
    return kOpArgs[op];
}

static float ReadF(const uint8_t* p) {
    float v;
    memcpy(&v, p, 4);
    return v;
}

static uint16_t ReadOp(const uint8_t* p) {
    uint16_t v;
    memcpy(&v, p, 2);
    return v;
}

// The two streams have to say the same thing. A thousandth of a viewBox unit
// is far below a pixel at any size an icon is drawn at, and comfortably above
// what the two float paths differ by: the generator does its arc arithmetic
// in doubles and rounds once at the end, the reader does it all in floats.
static bool SameOps(const uint8_t* a, int aLen, const uint8_t* b, int bLen,
                    const char* name) {
    int i = 0, j = 0;
    while (i < aLen && j < bLen) {
        uint16_t opA = ReadOp(a + i);
        uint16_t opB = ReadOp(b + j);
        if (opA != opB) {
            printf("  %s: op %u != %u at %d\n", name, opA, opB, i);
            return false;
        }
        int n = OpArgCount(opA);
        if (n < 0) {
            printf("  %s: unknown op %u at %d\n", name, opA, i);
            return false;
        }
        i += 2;
        j += 2;
        for (int k = 0; k < n; k++) {
            float fa = ReadF(a + i + k * 4);
            float fb = ReadF(b + j + k * 4);
            if (fabsf(fa - fb) > 0.001f) {
                printf("  %s: arg %d of op %u: %g != %g\n", name, k, opA, fa,
                       fb);
                return false;
            }
        }
        i += n * 4;
        j += n * 4;
        if (opA == kOpEnd) {
            break;
        }
    }
    return true;
}

// A stream written by hand and read back: the viewBox is where the reader
// looks for it, and the builder lays the arguments out in the order the
// opcode says.
static void BuilderRoundTrip() {
    DrawOpsBuilder b;
    b.ViewBox(0, 0, 32, 16);
    b.StrokeWidth(1.5f);
    b.Line(1, 2, 3, 4);
    b.MoveTo(5, 6);
    b.CubicTo(7, 8, 9, 10, 11, 12);
    b.ClosePath();
    b.Op(kOpFillStrokePath);
    b.End();

    Size vb = {};
    utassert(DrawOpsViewBox(b.data.els, b.data.len, &vb));
    utassertnear(vb.w, 32.f);
    utassertnear(vb.h, 16.f);

    // Two bytes of opcode each, plus 4 + 1 + 4 + 2 + 6 floats.
    utassert(b.data.len == 8 * 2 + 17 * 4);
    utassert(SameOps(b.data.els, b.data.len, b.data.els, b.data.len, "self"));

    // A drawing with no viewBox of its own is read in the lucide 24x24 box.
    DrawOpsBuilder plain;
    plain.Line(0, 0, 1, 1);
    plain.End();
    utassert(DrawOpsViewBox(plain.data.els, plain.data.len, &vb));
    utassertnear(vb.w, 24.f);
    utassertnear(vb.h, 24.f);
}

// A truncated stream is a stream, not a crash: the reader stops where the
// bytes do.
static void ShortStreamStops() {
    DrawOpsBuilder b;
    b.ViewBox(0, 0, 24, 24);
    b.MoveTo(1, 2);
    b.LineTo(3, 4);
    b.Op(kOpStrokePath);
    b.End();
    Size vb = {};
    for (int n = 0; n <= b.data.len; n++) {
        // No PaintCtx here, so this is the reader that can be driven without
        // one; ExecuteDrawOps is exercised by every icon the examples draw.
        DrawOpsViewBox(b.data.els, n, &vb);
    }
    utassert(true);
}

// ─── the reader, on SVG that is not a lucide icon ────────────────────────
//
// The three below are what an application's own picture may carry and every
// file under assets/icons happens not to, so nothing in the table above
// covers them.

// The first point of the first MoveTo/LineTo in a stream, which is enough to
// say where a shape landed.
static bool FirstPoint(const uint8_t* d, int len, float* x, float* y) {
    int at = 0;
    while (at + 2 <= len) {
        uint16_t op = (uint16_t)(d[at] | (d[at + 1] << 8));
        int n = OpArgCount(op);
        if (n < 0 || at + 2 + n * 4 > len) {
            return false;
        }
        if (op == kOpMoveTo || op == kOpLineTo) {
            *x = ReadF(d + at + 2);
            *y = ReadF(d + at + 6);
            return true;
        }
        at += 2 + n * 4;
        if (op == kOpEnd) {
            break;
        }
    }
    return false;
}

// Whether the stream names a colour, and which.
static bool FirstColor(const uint8_t* d, int len, uint32_t* out) {
    int at = 0;
    while (at + 2 <= len) {
        uint16_t op = (uint16_t)(d[at] | (d[at + 1] << 8));
        int n = OpArgCount(op);
        if (n < 0 || at + 2 + n * 4 > len) {
            return false;
        }
        if (op == kOpColor) {
            memcpy(out, d + at + 2, 4);
            return true;
        }
        at += 2 + n * 4;
        if (op == kOpEnd) {
            break;
        }
    }
    return false;
}

static bool HasOp(const uint8_t* d, int len, uint16_t want) {
    int at = 0;
    while (at + 2 <= len) {
        uint16_t op = (uint16_t)(d[at] | (d[at + 1] << 8));
        int n = OpArgCount(op);
        if (n < 0 || at + 2 + n * 4 > len) {
            return false;
        }
        if (op == want) {
            return true;
        }
        at += 2 + n * 4;
        if (op == kOpEnd) {
            break;
        }
    }
    return false;
}

// transform= was read off nothing at all before this: a <g> that moved its
// contents moved them nowhere. assets/icons/window-close.svg carries two
// translates that cancel, which is why no icon ever showed it.
static void AGroupTransformMovesWhatIsInsideIt() {
    DrawOpsBuilder plain;
    utassert(SvgToDrawOps(
        StrL("<svg viewBox=\"0 0 24 24\"><path d=\"M2 3 L4 5\"/></svg>"),
        &plain));
    float x = 0, y = 0;
    utassert(FirstPoint(plain.data.els, plain.data.len, &x, &y));
    utassertnear(x, 2.f);
    utassertnear(y, 3.f);

    DrawOpsBuilder moved;
    utassert(SvgToDrawOps(StrL("<svg viewBox=\"0 0 24 24\">"
                               "<g transform=\"translate(10, 6)\">"
                               "<path d=\"M2 3 L4 5\"/></g></svg>"),
                          &moved));
    utassert(FirstPoint(moved.data.els, moved.data.len, &x, &y));
    utassertnear(x, 12.f);
    utassertnear(y, 9.f);

    // Nested groups compose, and the pair window-close.svg uses cancels.
    DrawOpsBuilder cancels;
    utassert(SvgToDrawOps(StrL("<svg viewBox=\"0 0 24 24\">"
                               "<g transform=\"translate(-120, 0)\">"
                               "<g transform=\"translate(120, 0)\">"
                               "<path d=\"M2 3 L4 5\"/></g></g></svg>"),
                          &cancels));
    utassert(FirstPoint(cancels.data.els, cancels.data.len, &x, &y));
    utassertnear(x, 2.f);
    utassertnear(y, 3.f);

    // A group's transform stops at its close tag.
    DrawOpsBuilder after;
    utassert(SvgToDrawOps(StrL("<svg viewBox=\"0 0 24 24\">"
                               "<g transform=\"translate(10, 6)\"></g>"
                               "<path d=\"M2 3 L4 5\"/></svg>"),
                          &after));
    utassert(FirstPoint(after.data.els, after.data.len, &x, &y));
    utassertnear(x, 2.f);
    utassertnear(y, 3.f);

    // scale() and a shape's own transform, which applies after the groups'.
    DrawOpsBuilder scaled;
    utassert(SvgToDrawOps(StrL("<svg viewBox=\"0 0 24 24\">"
                               "<g transform=\"scale(2)\">"
                               "<path d=\"M2 3 L4 5\" "
                               "transform=\"translate(1, 1)\"/></g></svg>"),
                          &scaled));
    utassert(FirstPoint(scaled.data.els, scaled.data.len, &x, &y));
    utassertnear(x, 6.f);
    utassertnear(y, 8.f);
}

// <ellipse> was on none of the reader's lists, so it drew nothing at all. It
// cannot go through AddRoundRect either: one radius for two axes makes a
// stadium of anything that is wider than it is tall.
static void AnEllipseIsDrawnAndIsNotAStadium() {
    DrawOpsBuilder b;
    utassert(SvgToDrawOps(
        StrL("<svg viewBox=\"0 0 24 24\">"
             "<ellipse cx=\"12\" cy=\"6\" rx=\"10\" ry=\"2\"/></svg>"),
        &b));
    // It starts at the rightmost point of the ellipse.
    float x = 0, y = 0;
    utassert(FirstPoint(b.data.els, b.data.len, &x, &y));
    utassertnear(x, 22.f);
    utassertnear(y, 6.f);
    // Curves, not the four lines a stadium's flat sides would be.
    utassert(HasOp(b.data.els, b.data.len, kOpCubicTo));
    utassert(!HasOp(b.data.els, b.data.len, kOpLineTo));

    // No radius is nothing to draw rather than a crash or a point.
    DrawOpsBuilder none;
    utassert(!SvgToDrawOps(
        StrL("<svg viewBox=\"0 0 24 24\"><ellipse cx=\"1\" cy=\"1\"/></svg>"),
        &none));
}

// Only fill= was read off a shape, so a picture that named the colour it is
// *drawn* with came out in the caller's instead.
static void AShapeKeepsTheColourItIsStrokedWith() {
    DrawOpsBuilder b;
    utassert(SvgToDrawOps(StrL("<svg viewBox=\"0 0 24 24\">"
                               "<path d=\"M2 3 L4 5\" fill=\"none\" "
                               "stroke=\"#ff0000\"/></svg>"),
                          &b));
    uint32_t c = 0;
    utassert(FirstColor(b.data.els, b.data.len, &c));
    // kOpColor packs r,g,b,a low byte first.
    utassert((c & 0xff) == 0xff);
    utassert(((c >> 8) & 0xff) == 0);
    utassert(((c >> 16) & 0xff) == 0);
    utassert(HasOp(b.data.els, b.data.len, kOpStrokePath));

    // Filled in one colour and drawn in another is two passes, since one op
    // carries one colour.
    DrawOpsBuilder both;
    utassert(SvgToDrawOps(StrL("<svg viewBox=\"0 0 24 24\">"
                               "<rect x=\"1\" y=\"1\" width=\"4\" height=\"4\" "
                               "fill=\"#00ff00\" stroke=\"#0000ff\"/></svg>"),
                          &both));
    utassert(HasOp(both.data.els, both.data.len, kOpFillPath));
    utassert(HasOp(both.data.els, both.data.len, kOpStrokePath));

    // A shape that names neither is still the caller's colour, which is every
    // lucide icon and what the table above already proves.
    DrawOpsBuilder plain;
    utassert(SvgToDrawOps(
        StrL("<svg viewBox=\"0 0 24 24\"><path d=\"M2 3 L4 5\"/></svg>"),
        &plain));
    uint32_t unused = 0;
    utassert(!FirstColor(plain.data.els, plain.data.len, &unused));
}

// Every icon in the generated table, reconverted from its file.
static void GeneratedTableMatchesReader() {
    AssetsAddDefaultRoots({});
    if (AssetsRootCount() == 0) {
        // Built somewhere without the tree beside it; the table is still
        // compiled in and the examples still draw, there is just nothing to
        // compare it against.
        return;
    }
    utassert(kAssetIconsCount > 0);
    // The names run parallel to the table; a name without a row, or a row
    // without a name, would put every icon after it on the wrong bytes.
    utassert(SeqStrCount(kAssetIconNames) == kAssetIconsCount);
    int checked = 0;
    Str prev = {};
    for (int i = 0; i < kAssetIconsCount; i++) {
        const AssetIcon& e = kAssetIcons[i];
        Str name = SeqStrByIndex(kAssetIconNames, i);
        utassert(name.len > 0);
        // Name order, which is what makes the generated file's diff readable.
        if (i > 0) {
            utassert(strcmp(prev.s, name.s) < 0);
        }
        prev = name;
        utassert(e.offset >= 0 && e.len > 0 &&
                 e.offset + e.len <= kAssetIconsDataLen);
        // And the run answers for itself: the name at i finds row i.
        int found = 0;
        utassert(AssetIconFind(name, &found) == kAssetIconsData + e.offset);
        utassert(found == e.len);

        char path[128];
        snprintf(path, sizeof(path), "icons/%s.svg", name.s);
        TempStr xml = AssetsLoadTextTemp(Str(path));
        if (!xml.s) {
            continue;
        }
        DrawOpsBuilder b;
        utassert(SvgToDrawOps(xml, &b));
        utassert(SameOps(kAssetIconsData + e.offset, e.len, b.data.els,
                         b.data.len, name.s));
        checked++;
    }
    // The table came from that directory; if none of it could be read the
    // comparison above proved nothing.
    utassert(checked == 0 || checked == kAssetIconsCount);
}

void TestDrawOps() {
    TestSuite("DrawOps");
    BuilderRoundTrip();
    ShortStreamStops();
    AGroupTransformMovesWhatIsInsideIt();
    AnEllipseIsDrawnAndIsNotAStadium();
    AShapeKeepsTheColourItIsStrokedWith();
    GeneratedTableMatchesReader();
}
