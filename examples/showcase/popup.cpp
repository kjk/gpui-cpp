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
                        ->Fg(ExampleRgb(0xffffff)));
    El* content = nullptr;
    if (app->popupOpen) {
        content =
            Div(a)
                ->W(256)
                ->Pad(8)
                ->FlexCol()
                ->Bg(ExampleRgb(0xffffff))
                ->Border(1, ExampleRgb(0x171717))
                ->Child(TextEl(a, StrL("Anchored surface"))
                            ->Font(12)
                            ->Fg(ExampleRgb(0x171717)))
                ->Child(Div(a)->PadT(4)->Child(
                    TextEl(
                        a,
                        StrL(
                            "Popup positions content relative to its trigger."))
                        ->Font(14)
                        ->Fg(ExampleRgb(0x737373))
                        ->Wrap()
                        ->MaxW(240)));
    }
    return Popup::New(cx, StrL("example-popup"), trigger)
        ->Content(content)
        ->IntoEl();
}

SHOWCASE_PAGE(CompPopup, ShowcasePopup);
