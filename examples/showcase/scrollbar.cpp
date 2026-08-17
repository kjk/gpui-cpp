#include "Showcase.h"
#include "ui/Scrollbar.h"

El* ShowcaseScrollbar(ShowcaseApp* app, Arena* a) {
    El* list = Div(a)->FlexCol();
    for (int i = 1; i <= 20; i++) {
        list->Child(Div(a)
                        ->H(28)
                        ->PadX(8)
                        ->ItemsCenter()
                        ->JustifyBetween()
                        ->BorderB(1, Rgb(0xe5, 0xe7, 0xeb))
                        ->Child(TextEl(a, DupFmt(a, "Activity %d", i))
                                    ->Font(12)
                                    ->Fg(Rgb(0x17, 0x17, 0x17)))
                        ->Child(TextEl(a, i % 3 == 0 ? StrL("Completed")
                                                     : StrL("Pending"))
                                    ->Font(12)
                                    ->Fg(Rgb(0x17, 0x17, 0x17))));
    }
    const float viewH = 192;
    const float contentH = 20.f * 28.f;
    float maxS = contentH - viewH;
    if (maxS < 0) {
        maxS = 0;
    }
    if (app->exampleScroll < 0) {
        app->exampleScroll = 0;
    }
    if (app->exampleScroll > maxS) {
        app->exampleScroll = maxS;
    }
    float thumbH = viewH * viewH / contentH;
    if (thumbH < 48) {
        thumbH = 48;
    }
    float thumbY = (app->exampleScroll / maxS) * (viewH - thumbH);
    if (thumbY < 0) {
        thumbY = 0;
    }
    El* box = Scrollbar::New(a)
                  ->W(288)
                  ->H(viewH)
                  ->Border(1, Rgb(0x17, 0x17, 0x17))
                  ->ClipY()
                  ->ScrollY(app->exampleScroll)
                  ->Child(list);
    box->Child(Div(a)->Absolute()->Right(4)->Top(thumbY)->W(8)->H(thumbH)->Bg(
        Rgb(0xa3, 0xa3, 0xa3)));
    return box;
}

void ShowcaseScrollbarClick(ShowcaseApp* app, int id) {
    (void)app;
    (void)id;
}

SHOWCASE_PAGE(CompScrollbar, ShowcaseScrollbar, ShowcaseScrollbarClick);
