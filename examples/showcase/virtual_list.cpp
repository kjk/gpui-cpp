#include "Showcase.h"
#include "ui/VirtualList.h"

enum {
    kVirtCount = 100000
};
static const float kVirtRowH = 32;

El* ShowcaseVirtualList(ShowcaseApp* app, Arena* a) {
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
    El* list = Div(a)->FlexCol();
    if (first > 0) {
        list->Child(Div(a)->H((float)first * kVirtRowH));
    }
    for (int i = 0; i < visible; i++) {
        int ix = first + i;
        if (ix >= kVirtCount) {
            break;
        }
        list->Child(
            Div(a)
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
                        ->Child(
                            Div(a)
                                ->W(18)
                                ->H(18)
                                ->ItemsCenter()
                                ->JustifyCenter()
                                ->Border(1, Rgb(0x17, 0x17, 0x17))
                                ->Child(TextEl(a, DupFmt(a, "%d", (ix % 9) + 1))
                                            ->Font(11)
                                            ->Fg(Rgb(0x17, 0x17, 0x17))))
                        ->Child(TextEl(a, DupFmt(a, "Customer %06d", ix + 1))
                                    ->Font(12)
                                    ->Fg(Rgb(0x17, 0x17, 0x17))))
                ->Child(TextEl(a, DupFmt(a, "ID-%06d", 100000 + ix))
                            ->Font(12)
                            ->Fg(Rgb(0x17, 0x17, 0x17))));
    }
    float thumbH = viewH * viewH / contentH;
    if (thumbH < 48) {
        thumbH = 48;
    }
    float thumbY =
        maxS > 0 ? (app->virtualScroll / maxS) * (viewH - thumbH) : 0;
    El* box = VirtualList::New(a, StrL("example-virtual-list"))
                  ->W(288)
                  ->H(viewH)
                  ->Border(1, Rgb(0x17, 0x17, 0x17))
                  ->ClipY()
                  ->Child(list);
    box->Child(Div(a)->Absolute()->Right(4)->Top(thumbY)->W(8)->H(thumbH)->Bg(
        Rgb(0xa3, 0xa3, 0xa3)));
    return box;
}

void ShowcaseVirtualListClick(ShowcaseApp* app, int id) {
    (void)app;
    (void)id;
}

SHOWCASE_PAGE(CompVirtualList, ShowcaseVirtualList, ShowcaseVirtualListClick);
