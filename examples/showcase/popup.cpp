#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

static void OnPopup(ShowcaseApp* app, Ctx* cx, const ClickEvent*) {
    (void)app;
    app->popupOpen = !app->popupOpen;
    Notify(cx);
}

El* ShowcasePopup(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    El* trigger =
        Button::New(cx, StrL("popup-trigger"), false, Listen(cx, &OnPopup))
            ->H(28)
            ->PadX(12)
            ->ItemsCenter()
            ->JustifyCenter()
            ->Bg(Rgb(0, 0, 0))
            ->Child(TextEl(a, app->popupOpen ? StrL("Close popup")
                                             : StrL("Open popup"))
                        ->Font(12)
                        ->Fg(Rgb(0xff, 0xff, 0xff)));
    El* content = nullptr;
    if (app->popupOpen) {
        content =
            Div(a)
                ->W(256)
                ->Pad(8)
                ->FlexCol()
                ->Bg(Rgb(0xff, 0xff, 0xff))
                ->Border(1, Rgb(0x17, 0x17, 0x17))
                ->Child(TextEl(a, StrL("Anchored surface"))
                            ->Font(12)
                            ->Fg(Rgb(0x17, 0x17, 0x17)))
                ->Child(Div(a)->PadT(4)->Child(
                    TextEl(
                        a,
                        StrL(
                            "Popup positions content relative to its trigger."))
                        ->Font(14)
                        ->Fg(Rgb(0x73, 0x73, 0x73))
                        ->Wrap()
                        ->MaxW(240)));
    }
    return Popup::New(cx, StrL("example-popup"), trigger)
        ->Content(content)
        ->IntoEl();
}

SHOWCASE_PAGE(CompPopup, ShowcasePopup);
