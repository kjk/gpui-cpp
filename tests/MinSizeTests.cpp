/* min-width: auto is not min-width: 0.
 *
 * CSS gives an element that names no minimum the content-based automatic
 * minimum size, which is what stops a flex item shrinking below its own
 * content. `min_w_0()` is the other instruction — "this one may shrink past
 * its content" — and it is what a pane holding something wider than the
 * window says so the window's width still wins. `Style` kept both as a plain
 * float defaulting to zero, so the two were the same thing, and a table 4944
 * wide pushed the story gallery's content pane out to 4944 with it. */

#include "Test.h"

// A pane holding something wider than its parent, once with a minimum it
// never named and once with the zero it asked for.
static float PaneWidth(bool minZero) {
    Arena* a = ArenaNew();
    El* row = Div(a)->FlexRow()->W(kFill)->H(100);
    El* pane = Div(a)->FlexCol()->Grow()->ClipX();
    if (minZero) {
        pane->MinW(0);
    }
    pane->Child(Div(a)->W(4000)->H(50));
    row->Child(pane);
    LayoutEl(nullptr, row, 0, 0, 800, 100, 14, Rgba{});
    float w = pane->w;
    ArenaDelete(a);
    return w;
}

static void AnExplicitZeroMinimumLetsAPaneShrink() {
    TestSuite("gpui min-width");

    // The clip makes this one a scroll container, whose automatic minimum
    // size CSS already puts at zero, so both readings agree here.
    utassertnear(PaneWidth(true), 800.f);
    utassertnear(PaneWidth(false), 800.f);
}

// Without the clip there is nothing but the minimum to tell the two apart: an
// unnamed one is the content's 4000, an explicit zero is the parent's 800.
static void AnUnnamedMinimumIsTheContent() {
    Arena* a = ArenaNew();
    for (int minZero = 0; minZero < 2; minZero++) {
        El* row = Div(a)->FlexRow()->W(kFill)->H(100);
        El* pane = Div(a)->FlexCol()->Grow();
        if (minZero) {
            pane->MinW(0);
        }
        pane->Child(Div(a)->W(4000)->H(50));
        row->Child(pane);
        LayoutEl(nullptr, row, 0, 0, 800, 100, 14, Rgba{});
        utassertnear(pane->w, minZero ? 800.f : 4000.f);
    }
    ArenaDelete(a);
}

// The same pair on the block axis, which is where a scrolling page needs it.
static void TheHeightAxisReadsTheSameWay() {
    Arena* a = ArenaNew();
    for (int minZero = 0; minZero < 2; minZero++) {
        El* col = Div(a)->FlexCol()->W(100)->H(200);
        El* pane = Div(a)->FlexCol()->Grow();
        if (minZero) {
            pane->MinH(0);
        }
        pane->Child(Div(a)->W(100)->H(900));
        col->Child(pane);
        LayoutEl(nullptr, col, 0, 0, 100, 200, 14, Rgba{});
        utassertnear(pane->h, minZero ? 200.f : 900.f);
    }
    ArenaDelete(a);
}

// A border takes room, the way GPUI hands `Style::border_widths` to taffy and
// CSS's `box-sizing: border-box` says: the box keeps the size it was given
// and the content inside it moves in by the width. The port fed taffy no
// border at all, so every bordered box was its border narrower and shorter
// than upstream's — a card 47 tall where Rust's was 49.
static void ABorderTakesRoom() {
    Arena* a = ArenaNew();
    // Content-sized: the box grows by the border it carries.
    El* plain = Div(a)->FlexCol()->Child(Div(a)->W(100)->H(40));
    LayoutEl(nullptr, plain, 0, 0, 0, 0, 14, Rgba{});
    utassertnear(plain->h, 40.f);
    utassertnear(plain->w, 100.f);

    El* bordered = Div(a)->FlexCol()->Border(1, Rgb(0, 0, 0));
    bordered->Child(Div(a)->W(100)->H(40));
    LayoutEl(nullptr, bordered, 0, 0, 0, 0, 14, Rgba{});
    utassertnear(bordered->h, 42.f);
    utassertnear(bordered->w, 102.f);
    ArenaDelete(a);
}

// A box of a named size keeps it, and the content is what moves in — which is
// why a 16px checkbox stays 16 and its tick is drawn in the 14 inside.
static void AFixedBoxKeepsItsSizeAndInsetsItsContent() {
    Arena* a = ArenaNew();
    El* box = Div(a)->FlexCol()->W(16)->H(16)->Border(1, Rgb(0, 0, 0));
    El* fill = Div(a)->W(kFill)->H(kFill);
    box->Child(fill);
    LayoutEl(nullptr, box, 0, 0, 0, 0, 14, Rgba{});
    utassertnear(box->w, 16.f);
    utassertnear(box->h, 16.f);
    utassertnear(fill->w, 14.f);
    utassertnear(fill->h, 14.f);
    // And the content starts inside the stroke rather than under it.
    utassertnear(fill->x - box->x, 1.f);
    utassertnear(fill->y - box->y, 1.f);
    ArenaDelete(a);
}

// The per-edge widths are their own, and an element carrying both the
// all-round width and a heavier edge reserves the larger of the two rather
// than their sum — paint draws both strokes over the same pixels.
static void AnEdgeReservesTheLargerOfTheTwoWidths() {
    Arena* a = ArenaNew();
    El* onlyBottom = Div(a)->FlexCol()->BorderB(2, Rgb(0, 0, 0));
    onlyBottom->Child(Div(a)->W(50)->H(10));
    LayoutEl(nullptr, onlyBottom, 0, 0, 0, 0, 14, Rgba{});
    utassertnear(onlyBottom->h, 12.f);
    utassertnear(onlyBottom->w, 50.f);

    El* both =
        Div(a)->FlexCol()->Border(1, Rgb(0, 0, 0))->BorderB(3, Rgb(0, 0, 0));
    both->Child(Div(a)->W(50)->H(10));
    LayoutEl(nullptr, both, 0, 0, 0, 0, 14, Rgba{});
    // 1 on top, 3 at the bottom -- not 1 + 3 at the bottom.
    utassertnear(both->h, 14.f);
    ArenaDelete(a);
}

void TestMinSize() {
    AnExplicitZeroMinimumLetsAPaneShrink();
    AnUnnamedMinimumIsTheContent();
    TheHeightAxisReadsTheSameWay();
    ABorderTakesRoom();
    AFixedBoxKeepsItsSizeAndInsetsItsContent();
    AnEdgeReservesTheLargerOfTheTwoWidths();
}
