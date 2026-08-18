#include "Story.h"

static const char* kVlDatasets[] = {"Standard", "Wide", "Stress", "Short"};
static const int kVlColumns[] = {30, 100, 100, 10};
static const char* kVlAxes[] = {"Both", "Vertical", "Horizontal"};

// ITEM_SIZE in the Rust story.
static const float kCellW = 100;
static const float kCellH = 30;

enum {
    VlMenuDataset = 1,
    VlMenuAxis
};

enum {
    VlActDataset = 300, // + index
    VlActAxis = 320     // + index
};

struct VirtualListStory {
    int dataset = 0;
    int axis = 0;
    int openMenu = 0;
    float scrollY = 0;

    static El* Render(VirtualListStory* self, Ctx* cx);
};

static void VlMenuOpen(VirtualListStory* self, Ctx* cx, const ClickEvent*,
                       intptr_t which) {
    self->openMenu = self->openMenu == (int)which ? 0 : (int)which;
    Notify(cx);
}
static void VlMenuAct(VirtualListStory* self, Ctx* cx, const ClickEvent*,
                      intptr_t act) {
    if (act >= VlActAxis) {
        self->axis = (int)(act - VlActAxis);
    } else {
        self->dataset = (int)(act - VlActDataset);
    }
    self->openMenu = 0;
    Notify(cx);
}
static void VlScrollTo(VirtualListStory* self, Ctx* cx, const ClickEvent*,
                       intptr_t row) {
    self->scrollY = (float)row * (kCellH + 4);
    Notify(cx);
}

El* VirtualListStory::Render(VirtualListStory* self, Ctx* cx) {
    WinSize win = WindowSize(cx->win);
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    Listener openMenu = Listen(cx, &VlMenuOpen);
    Listener act = Listen(cx, &VlMenuAct);
    Listener scrollTo = Listen(cx, &VlScrollTo);

    El* page = Div(a)->FlexCol()->Gap(16)->W(kFill);

    // One toolbar group: the two dropdowns and the four scroll buttons.
    El* toolbarRow = Div(a)->FlexRow()->W(kFill)->JustifyEnd()->ItemsStart();
    El* group = StoryToolbarGroup(cx);
    StoryToolbarOpt datasets[4];
    for (int i = 0; i < 4; i++) {
        datasets[i].label = kVlDatasets[i];
        datasets[i].checked = self->dataset == i;
        datasets[i].act = VlActDataset + i;
    }
    group->Child(StoryToolbarDropdown(
        cx, StrL("virtual-list-dataset"),
        StoryFmt(cx, "Dataset: %s", kVlDatasets[self->dataset]),
        self->openMenu == VlMenuDataset, ListenerArg(openMenu, VlMenuDataset),
        datasets, 4, act));
    group->Child(StoryToolbarDivider(cx));
    StoryToolbarOpt axes[3];
    for (int i = 0; i < 3; i++) {
        axes[i].label = kVlAxes[i];
        axes[i].checked = self->axis == i;
        axes[i].act = VlActAxis + i;
    }
    group->Child(
        StoryToolbarDropdown(cx, StrL("virtual-list-axis"),
                             StoryFmt(cx, "Axis: %s", kVlAxes[self->axis]),
                             self->openMenu == VlMenuAxis,
                             ListenerArg(openMenu, VlMenuAxis), axes, 3, act));
    struct ScrollBtn {
        const char* id;
        const char* label;
        int row;
    };
    static const ScrollBtn kScrollBtns[] = {{"scroll-to0", "Top", 0},
                                            {"scroll-to1", "Row 50", 50},
                                            {"scroll-to2", "Center 25", 25},
                                            {"scroll-to-bottom", "Bottom", 90}};
    for (size_t i = 0; i < sizeof(kScrollBtns) / sizeof(kScrollBtns[0]); i++) {
        group->Child(StoryToolbarDivider(cx));
        El* btn = Div(a)
                      ->H(24)
                      ->PadX(8)
                      ->ItemsCenter()
                      ->JustifyCenter()
                      ->HoverBg(th.muted)
                      ->Child(StoryTxt(cx, Str(kScrollBtns[i].label), 14,
                                       th.foreground));
        btn->Click(HashClickId(Str(kScrollBtns[i].id)))
            ->OnClick(ListenerArg(scrollTo, kScrollBtns[i].row));
        group->Child(btn);
    }
    toolbarRow->Child(group);
    page->Child(toolbarRow);

    int rows = 24;
    page->Child(StoryTxt(cx, StoryFmt(cx, "Visible: 0..%d", rows - 1), 16,
                         th.foreground));

    // The grid: rows of 100x30 cells on the secondary background.
    El* box = Div(a)
                  ->FlexCol()
                  ->W(kFill)
                  ->H(win.dipH - 280)
                  ->Pad(16)
                  ->Gap(4)
                  ->Border(1, th.border)
                  ->ClipY()
                  ->ScrollY(self->scrollY);
    int columns = kVlColumns[self->dataset];
    for (int r = 0; r < rows; r++) {
        El* row = Div(a)->FlexRow()->Gap(4)->ItemsCenter();
        for (int c = 0; c < columns && c < 7; c++) {
            row->Child(Div(a)
                           ->FlexRow()
                           ->W(kCellW)
                           ->H(kCellH)
                           ->ItemsCenter()
                           ->JustifyCenter()
                           ->Bg(th.secondary)
                           ->Child(StoryTxt(cx,
                                            c == 0 ? StoryFmt(cx, "row: %d", r)
                                                   : StoryFmt(cx, "%d", c),
                                            14, th.foreground)));
        }
        box->Child(row);
    }
    page->Child(box);
    return page;
}

STORY_PAGE(StoryVirtualList, VirtualListStory);
