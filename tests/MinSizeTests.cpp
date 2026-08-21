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

void TestMinSize() {
    AnExplicitZeroMinimumLetsAPaneShrink();
    AnUnnamedMinimumIsTheContent();
    TheHeightAxisReadsTheSameWay();
}
