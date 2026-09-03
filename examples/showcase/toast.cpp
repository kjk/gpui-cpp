#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

static void ShowToast(ShowcaseApp* app, Ctx* cx, const ClickEvent*) {
    app->toastOn = true;
    Notify(cx);
}

static void HideToast(ShowcaseApp* app, Ctx* cx, const ClickEvent*) {
    app->toastOn = false;
    Notify(cx);
}

El* ShowcaseToast(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    El* btn = Button::New(cx, StrL("show-toast"))
                  ->OnClick(Listen(cx, &ShowToast))
                  ->H(28)
                  ->PadX(8)
                  ->Shrink0()
                  ->ItemsCenter()
                  ->JustifyCenter()
                  ->Border(1, ExampleRgb(0x171717))
                  ->Bg(ExampleRgb(0xffffff))
                  ->Child(TextEl(a, StrL("Save changes"))
                              ->Font(12)
                              ->Fg(ExampleRgb(0x171717)));
    El* box = Div(a)->W(288)->H(158)->ClipY()->Child(
        Div(a)->W(kFill)->H(kFill)->ItemsCenter()->JustifyCenter()->Child(btn));
    if (app->toastOn) {
        box->Child(
            Toast::New(cx, StrL("example-toast"))
                ->IntoEl()
                ->Absolute()
                ->Right(0)
                ->Bottom(0)
                ->W(256)
                ->Pad(8)
                ->FlexCol()
                ->Border(1, ExampleRgb(0x171717))
                ->Bg(ExampleRgb(0xffffff))
                ->Child(
                    Div(a)
                        ->FlexRow()
                        ->JustifyBetween()
                        ->W(kFill)
                        ->Child(TextEl(a, StrL("Changes saved"))
                                    ->Font(12)
                                    ->Fg(ExampleRgb(0x171717))
                                    ->Semibold())
                        ->Child(Button::New(cx, StrL("dismiss-toast"))
                                    ->OnClick(Listen(cx, &HideToast))
                                    ->W(24)
                                    ->H(24)
                                    ->ItemsCenter()
                                    ->JustifyCenter()
                                    ->Child(TextEl(a, StrL("×"))
                                                ->Font(14)
                                                ->Fg(ExampleRgb(0x171717)))))
                ->Child(Div(a)->PadT(4)->Child(
                    TextEl(a, StrL("Your preferences are now up to date."))
                        ->Font(12)
                        ->Fg(ExampleRgb(0x737373)))));
    }
    return box;
}

SHOWCASE_PAGE(CompToast, ShowcaseToast);
