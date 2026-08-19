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

    static El* Render(ScrollbarStory* self, Ctx* cx);
};

static void ToggleDatasetMenu(ScrollbarStory* self, Ctx* cx,
                              const ClickEvent*) {
    self->menuOpen = !self->menuOpen;
    Notify(cx);
}
static void PickDataset(ScrollbarStory* self, Ctx* cx, const ClickEvent*,
                        intptr_t act) {
    self->dataset = (int)(act - ScrollActDataset);
    self->menuOpen = false;
    self->scrollY = 0;
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
                    ->ScrollY(self->scrollY);
    int count = kDatasets[self->dataset].count;
    // Only what can show is built; the Rust list is virtualized.
    int visible = count < 40 ? count : 40;
    for (int i = 0; i < visible; i++) {
        frame->Child(
            Div(a)
                ->H(kItemHeight)
                ->W(kFill)
                ->PadT(4)
                // .items_center() in the Rust story: the row is 50px tall and
                // the chip inside keeps its own height rather than stretching
                // to fill it.
                ->ItemsCenter()
                ->Child(Div(a)
                            ->W(kFill)
                            ->Pad(8)
                            ->Bg(th.secondary)
                            ->Child(StoryTxt(cx, StoryFmt(cx, "Item %d", i), 14,
                                             th.foreground))));
    }
    page->Child(frame);
    return page;
}

STORY_PAGE(StoryScrollbar, ScrollbarStory);
