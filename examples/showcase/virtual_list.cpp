#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

enum {
    kVirtCount = 100000
};
static const float kVirtRowH = 32;

El* ShowcaseVirtualList(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    const float viewH = 192;
    const float contentH = (float)kVirtCount * kVirtRowH;
    if (app->virtualScroll < 0) {
        app->virtualScroll = 0;
    }
    float maxS = contentH - viewH;
    if (maxS < 0) {
        maxS = 0;
    }
    if (app->virtualScroll > maxS) {
        app->virtualScroll = maxS;
    }
    int first = (int)(app->virtualScroll / kVirtRowH);
    if (first < 0) {
        first = 0;
    }
    int visible = (int)(viewH / kVirtRowH) + 2;
    // v_virtual_list renders only the visible range and translates it by the
    // scroll offset. The rendered window starts at the first item's own
    // offset, so it rides up as the offset grows; a spacer above the rows
    // would push them out of the box instead of scrolling them.
    El* list = Div(a)
                   ->FlexCol()
                   ->Absolute()
                   ->Left(0)
                   ->Top((float)first * kVirtRowH - app->virtualScroll)
                   ->W(kFill);
    for (int i = 0; i < visible; i++) {
        int ix = first + i;
        if (ix >= kVirtCount) {
            break;
        }
        list->Child(
            Div(a)
                ->W(kFill)
                ->H((float)kVirtRowH)
                ->PadX(8)
                ->ItemsCenter()
                ->JustifyBetween()
                ->BorderB(1, Rgb(0x17, 0x17, 0x17))
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
                                    ->Border(1, Rgb(0x17, 0x17, 0x17))
                                    ->Child(TextEl(a, DupFmt(cx, "%d",
                                                             (ix % 9) + 1))
                                                ->Font(11)
                                                ->Fg(Rgb(0x17, 0x17, 0x17))))
                        ->Child(TextEl(a, DupFmt(cx, "Customer %06d", ix + 1))
                                    ->Font(12)
                                    ->Fg(Rgb(0x17, 0x17, 0x17))))
                ->Child(TextEl(a, DupFmt(cx, "ID-%06d", 100000 + ix))
                            ->Font(12)
                            ->Fg(Rgb(0x17, 0x17, 0x17))));
    }
    float thumbH = viewH * viewH / contentH;
    if (thumbH < 48) {
        thumbH = 48;
    }
    float thumbY =
        maxS > 0 ? (app->virtualScroll / maxS) * (viewH - thumbH) : 0;
    El* box = VirtualList::New(cx, StrL("example-virtual-list"))
                  ->W(288)
                  ->H(viewH)
                  ->Border(1, Rgb(0x17, 0x17, 0x17))
                  ->ClipY()
                  ->Child(list);
    box->Child(Div(a)->Absolute()->Right(4)->Top(thumbY)->W(8)->H(thumbH)->Bg(
        Rgb(0xa3, 0xa3, 0xa3)));
    return box;
}

SHOWCASE_PAGE(CompVirtualList, ShowcaseVirtualList);
