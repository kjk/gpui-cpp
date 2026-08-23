#include "Story.h"

// The four datasets the Rust story cycles through.
struct ScrollDataset {
    const char* label;
    int count;
};

static const ScrollDataset kDatasets[] = {
    {"Standard", 5000},
    {"Wide", 100},
    {"Stress", 500000},
    {"Short", 5},
};

// ITEM_HEIGHT in the Rust story.
static const float kItemHeight = 50;

enum {
    ScrollActDataset = 200
};

struct ScrollbarStory {
    int dataset = 0;
    bool menuOpen = false;
    float scrollY = 0;
    // The grid below the list scrolls both ways, which is what
    // ScrollbarAxis::Both is for.
    float gridX = 0;
    float gridY = 0;

    static El* Render(ScrollbarStory* self, Ctx* cx);
};

// The bar reports the offset it worked out; the view owns scrollY, so it is
// the one that stores it. Rust's scrollbar writes into a shared ScrollHandle
// instead, which comes to the same thing.
static void ScrollTo(ScrollbarStory* self, Ctx* cx, const ScrollEvent* ev) {
    self->scrollY = ev->offsetY;
    Notify(cx);
}

// Both offsets at once: the box reports where it should now be, whichever bar
// or wheel moved it.
static void ScrollGrid(ScrollbarStory* self, Ctx* cx, const ScrollEvent* ev) {
    self->gridX = ev->offsetX;
    self->gridY = ev->offsetY;
    Notify(cx);
}

static void ToggleDatasetMenu(ScrollbarStory* self, Ctx* cx,
                              const ClickEvent*) {
    self->menuOpen = !self->menuOpen;
    Notify(cx);
}
static void PickDataset(ScrollbarStory* self, Ctx* cx, const ClickEvent*,
                        intptr_t act) {
    // ToolbarCloseAll names no row: it only wants the menu shut.
    if (act >= ScrollActDataset) {
        self->dataset = (int)(act - ScrollActDataset);
        self->scrollY = 0;
    }
    self->menuOpen = false;
    Notify(cx);
}

El* ScrollbarStory::Render(ScrollbarStory* self, Ctx* cx) {
    WinSize win = WindowSize(cx->win);
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* page = Div(a)->FlexCol()->Gap(16)->W(kFill);

    // story_toolbar_group() with the dataset dropdown.
    El* toolbarRow = Div(a)->FlexRow()->W(kFill)->JustifyEnd()->ItemsStart();
    El* group = StoryToolbarGroup(cx);
    StoryToolbarOpt rows[4];
    for (int i = 0; i < 4; i++) {
        rows[i].label = kDatasets[i].label;
        rows[i].checked = self->dataset == i;
        rows[i].act = ScrollActDataset + i;
    }
    group->Child(StoryToolbarDropdown(
        cx, StrL("scrollbar-dataset"),
        StoryFmt(cx, "Dataset: %s", kDatasets[self->dataset].label),
        self->menuOpen, Listen(cx, &ToggleDatasetMenu), rows, 4,
        Listen(cx, &PickDataset)));
    toolbarRow->Child(group);
    page->Child(toolbarRow);

    // The list fills what is left of the page, inside a bordered frame.
    El* frame = Div(a)
                    ->FlexCol()
                    ->W(kFill)
                    // flex_1 in a scrolling page: take what is left of the
                    // window below the toolbar.
                    ->H(win.dipH - 230)
                    ->PadX(12)
                    ->PadY(4)
                    ->Border(1, th.border)
                    ->ClipY()
                    ->ScrollY(self->scrollY)
                    ->ScrollId(HashClickId(StrL("scrollbar-list")))
                    ->OnScroll(Listen(cx, &ScrollTo));
    int count = kDatasets[self->dataset].count;
    // The Rust list is virtualized, and so is this one: only the rows the
    // frame can show are built, with a spacer at each end standing in for the
    // rest — which is what makes half a million of them cost nothing.
    float frameH = win.dipH - 230;
    VirtualRange range =
        VirtualListVisibleRows(count, kItemHeight, self->scrollY, frameH);
    if (range.first > 0) {
        frame->Child(Div(a)->W(kFill)->H((float)range.first * kItemHeight));
    }
    for (int i = range.first; i < range.end; i++) {
        frame->Child(
            Div(a)
                ->H(kItemHeight)
                ->W(kFill)
                // A row of a uniform_list keeps its height: the frame holds
                // far more than it can show, and a flex child that shrinks
                // would give every row back a seventh of it.
                ->Shrink0()
                ->PadT(4)
                // .items_center() in the Rust story: the row is 50px tall and
                // the chip inside keeps its own height rather than stretching
                // to fill it.
                ->ItemsCenter()
                ->Child(Div(a)
                            ->W(kFill)
                            ->Pad(8)
                            ->Bg(th.tokens.secondary)
                            ->Child(StoryTxt(cx, StoryFmt(cx, "Item %d", i), 14,
                                             th.foreground))));
    }
    if (range.end < count) {
        frame->Child(
            Div(a)->W(kFill)->H((float)(count - range.end) * kItemHeight));
    }
    page->Child(frame);

    // ScrollbarAxis::Both: a grid wider and taller than its frame, with a bar
    // down the side and another along the bottom.
    El* grid = Div(a)->FlexCol();
    for (int r = 0; r < 20; r++) {
        El* row = Div(a)->FlexRow()->Gap(4)->PadY(2);
        for (int c = 0; c < 12; c++) {
            row->Child(Div(a)
                           ->W(120)
                           ->H(28)
                           ->ItemsCenter()
                           ->JustifyCenter()
                           ->Bg(th.tokens.secondary)
                           ->Radius(th.radius)
                           ->Child(StoryTxt(cx, StoryFmt(cx, "%d:%d", r, c), 13,
                                            th.foreground)));
        }
        grid->Child(row);
    }
    El* both = StorySection(cx, "Both Axes",
                            "A scroll area with a bar on each axis. The "
                            "wheel scrolls whichever box it is over.");
    StorySectionAdd(both, component::Scrollable::New(cx, StrL("scrollbar-grid"))
                              ->Axis(component::ScrollAxis::Both)
                              ->H(200)
                              ->ScrollX(self->gridX)
                              ->ScrollY(self->gridY)
                              ->OnScroll(Listen(cx, &ScrollGrid))
                              ->Child(grid)
                              ->IntoEl());
    page->Child(both);

    return page;
}

STORY_PAGE(StoryScrollbar, ScrollbarStory);
