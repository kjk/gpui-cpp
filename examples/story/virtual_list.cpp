#include "Story.h"

static const char* kVlDatasets[] = {"Standard", "Wide", "Stress", "Short"};
// How many rows each dataset has, and how many columns a row shows.
static const int kVlRows[] = {500, 500, 50000, 20};
static const int kVlColumns[] = {30, 100, 100, 10};
static const char* kVlAxes[] = {"Both", "Vertical", "Horizontal"};

// ITEM_SIZE in the Rust story.
static const float kCellW = 100;
static const float kCellH = 30;
// The row height the list lays out with: the cell plus the gap under it.
static const float kRowH = kCellH + 4;

enum {
    VlMenuDataset = 1,
    VlMenuAxis
};

enum {
    VlActDataset = 300, // + index
    VlActAxis = 320     // + index
};

// The scroll buttons, and what each asks the handle for.
struct VlScrollBtn {
    const char* id;
    const char* label;
    int row;
    ScrollStrategy strategy;
};

static const VlScrollBtn kVlScrollBtns[] = {
    {"scroll-to0", "Top", 0, ScrollStrategy::Top},
    {"scroll-to1", "Row 50", 50, ScrollStrategy::Top},
    {"scroll-to2", "Center 25", 25, ScrollStrategy::Center},
    {"scroll-to-bottom", "Bottom", -1, ScrollStrategy::Top}};

struct VirtualListStory {
    int dataset = 0;
    int axis = 0;
    int openMenu = 0;
    // The handle the page holds: Rust's VirtualListScrollHandle, which is
    // where the list's offset lives and where a scroll_to_item waits until
    // the list is next laid out.
    VirtualListScrollHandle handle = {};
    // The rows the last layout built, which is what the page reports.
    VirtualRange visible = {};

    static El* Render(VirtualListStory* self, Ctx* cx);
    static void OnScroll(VirtualListStory* self, Ctx* cx,
                         const ScrollEvent* ev);
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

// The buttons ask the handle, not the list: nothing here knows where a row is
// until the list is laid out, which is exactly what Rust defers.
static void VlScrollTo(VirtualListStory* self, Ctx* cx, const ClickEvent*,
                       intptr_t which) {
    const VlScrollBtn& b = kVlScrollBtns[which];
    if (b.row < 0) {
        VirtualListScrollToBottomDeferred(&self->handle);
    } else {
        VirtualListScrollToItemDeferred(&self->handle, b.row, b.strategy);
    }
    Notify(cx);
}

void VirtualListStory::OnScroll(VirtualListStory* self, Ctx* cx,
                                const ScrollEvent* ev) {
    self->handle.offset = ev->offsetY;
    Notify(cx);
}

// The row the list builds for each index: one cell naming the row, and a run
// of numbered cells after it.
static int gVlColumns = 7;

static El* VlRow(Arena* a, int ix) {
    El* row = Div(a)->FlexRow()->Gap(4)->ItemsCenter()->H(kRowH);
    for (int c = 0; c < gVlColumns && c < 7; c++) {
        Str label =
            c == 0 ? StrDup(a, fmt("row: %d", ix)) : StrDup(a, fmt("%d", c));
        row->Child(Div(a)
                       ->FlexRow()
                       ->W(kCellW)
                       ->H(kCellH)
                       ->ItemsCenter()
                       ->JustifyCenter()
                       ->Bg(RgbaHex(0xf1f5f9ff))
                       ->Child(TextEl(a, label)->Font(14)));
    }
    return row;
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
    for (int i = 0; i < 4; i++) {
        group->Child(StoryToolbarDivider(cx));
        El* btn = Div(a)
                      ->H(24)
                      ->PadX(8)
                      ->ItemsCenter()
                      ->JustifyCenter()
                      ->HoverBg(th.muted)
                      ->Child(StoryTxt(cx, Str(kVlScrollBtns[i].label), 14,
                                       th.foreground));
        btn->Click(HashClickId(Str(kVlScrollBtns[i].id)))
            ->OnClick(ListenerArg(scrollTo, i));
        group->Child(btn);
    }
    toolbarRow->Child(group);
    page->Child(toolbarRow);

    // The rows the last layout built, which is the range the list reports.
    page->Child(StoryTxt(
        cx,
        StoryFmt(cx, "Visible: %d..%d", self->visible.first, self->visible.end),
        16, th.foreground));

    gVlColumns = kVlColumns[self->dataset];
    int rows = kVlRows[self->dataset];
    float viewH = win.dipH - 280;
    El* list = component::VirtualList::New(cx, rows)
                   ->Id(StrL("virtual-list-story"))
                   ->RowH(kRowH)
                   ->ViewH(viewH)
                   ->Handle(&self->handle)
                   ->Scroll(1, Listen(cx, &VirtualListStory::OnScroll))
                   ->Row(VlRow)
                   ->IntoEl();
    self->visible = VirtualListHandleRange(&self->handle, nullptr, rows, kRowH);
    page->Child(Div(a)
                    ->FlexCol()
                    ->W(kFill)
                    ->Pad(16)
                    ->Border(1, th.border)
                    ->Child(list));
    return page;
}

STORY_PAGE(StoryVirtualList, VirtualListStory);
