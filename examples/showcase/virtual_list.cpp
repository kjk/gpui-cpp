#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

enum {
    kVirtCount = 100000
};
static const float kVirtRowH = 32;

// `v_virtual_list(cx.entity(), "example-virtual-list", sizes, |_, range, _, _|
// ...)`: the closure builds the rows the range names and nothing else. Rust
// gets the whole range at once; this is asked for one row at a time.
static El* VirtRow(void* user, Ctx* cx, int ix) {
    (void)user;
    Arena* a = cx->a;
    return Div(a)
        ->W(kFill)
        ->H(kVirtRowH)
        ->PadX(8)
        ->ItemsCenter()
        ->JustifyBetween()
        ->BorderB(1, ScInk())
        ->Child(
            Div(a)
                ->FlexRow()
                ->ItemsCenter()
                ->Gap(8)
                ->Child(Div(a)
                            ->W(18)
                            ->H(18)
                            ->ItemsCenter()
                            ->JustifyCenter()
                            ->Border(1, ScInk())
                            ->Child(TextEl(a, DupFmt(cx, "%d", (ix % 9) + 1))
                                        ->Font(11)
                                        ->Fg(ScInk())))
                ->Child(TextEl(a, DupFmt(cx, "Customer %06d", ix + 1))
                            ->Font(12)
                            ->Fg(ScInk())))
        ->Child(TextEl(a, DupFmt(cx, "ID-%06d", 100000 + ix))
                    ->Font(12)
                    ->Fg(ScInk()));
}

// `.track_scroll(&self.virtual_scroll)`: the list reports where it should be
// and the page keeps it.
static void OnVirtualScroll(ShowcaseApp* app, Ctx* cx, const ScrollEvent* ev) {
    app->virtualScroll = ev->offsetY;
    Notify(cx);
}

El* ShowcaseVirtualList(ShowcaseApp* app, Ctx* cx) {
    // Which rows to build, where the spacers go and how long the thumb is are
    // the list's own business. The page used to do all three, and the thumb
    // it drew could not be dragged.
    VirtualListOpts o;
    o.count = kVirtCount;
    o.rowH = kVirtRowH;
    o.viewH = 192;
    o.scrollY = app->virtualScroll;
    o.scrollId = HashClickId(StrL("example-virtual-list"));
    o.onScroll = Listen(cx, &OnVirtualScroll);
    o.axis = ScrollAxis::Vertical;
    o.row = &VirtRow;
    return VirtualList::New(cx, StrL("example-virtual-list"), o)
        ->W(288)
        ->Border(1, ScInk());
}

SHOWCASE_PAGE(CompVirtualList, ShowcaseVirtualList);
