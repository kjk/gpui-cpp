#include "Showcase.h"
#include "ui/Button.h"
#include "ui/Toast.h"

enum {
    ClickToastShow = 550,
    ClickToastDismiss = 551
};

El* ShowcaseToast(ShowcaseApp* app, Arena* a) {
    El* btn = Button::New(a, StrL("show-toast"), ClickToastShow)
                  ->H(28)
                  ->PadX(8)
                  ->Shrink0()
                  ->ItemsCenter()
                  ->JustifyCenter()
                  ->Border(1, Rgb(0x17, 0x17, 0x17))
                  ->Bg(Rgb(0xff, 0xff, 0xff))
                  ->Child(TextEl(a, StrL("Save changes"))
                              ->Font(12)
                              ->Fg(Rgb(0x17, 0x17, 0x17)));
    El* box = Div(a)->W(288)->H(158)->ClipY()->Child(
        Div(a)->W(kFill)->H(kFill)->ItemsCenter()->JustifyCenter()->Child(btn));
    if (app->toastOn) {
        box->Child(
            Toast::New(a, StrL("example-toast"))
                ->Absolute()
                ->Right(0)
                ->Bottom(0)
                ->W(256)
                ->Pad(8)
                ->FlexCol()
                ->Border(1, Rgb(0x17, 0x17, 0x17))
                ->Bg(Rgb(0xff, 0xff, 0xff))
                ->Child(
                    Div(a)
                        ->FlexRow()
                        ->JustifyBetween()
                        ->W(kFill)
                        ->Child(TextEl(a, StrL("Changes saved"))
                                    ->Font(12)
                                    ->Fg(Rgb(0x17, 0x17, 0x17))
                                    ->Semibold())
                        ->Child(Button::New(a, StrL("dismiss-toast"),
                                            ClickToastDismiss)
                                    ->W(24)
                                    ->H(24)
                                    ->ItemsCenter()
                                    ->JustifyCenter()
                                    ->Child(TextEl(a, StrL("×"))
                                                ->Font(14)
                                                ->Fg(Rgb(0x17, 0x17, 0x17)))))
                ->Child(Div(a)->PadT(4)->Child(
                    TextEl(a, StrL("Your preferences are now up to date."))
                        ->Font(12)
                        ->Fg(Rgb(0x73, 0x73, 0x73)))));
    }
    return box;
}

void ShowcaseToastClick(ShowcaseApp* app, int id) {
    if (id == ClickToastShow) {
        app->toastOn = true;
    } else if (id == ClickToastDismiss) {
        app->toastOn = false;
    }
}

SHOWCASE_PAGE(CompToast, ShowcaseToast, ShowcaseToastClick);
