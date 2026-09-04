#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

static void OnOtp(ShowcaseApp* app, Ctx* cx, const ClickEvent*) {
    (void)app;
    app->otpOn = true;
    app->input.focused = false;
    Notify(cx);
}

El* ShowcaseOtpInput(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    int active = app->otpLen;
    if (active > 5) {
        active = 5;
    }
    El* cells = OtpInput::New(cx, StrL("example-otp"))
                    ->OnClick(Listen(cx, &OnOtp))
                    ->FlexRow()
                    ->Gap(4);
    for (int i = 0; i < 6; i++) {
        Str ch = i < app->otpLen ? Str(app->otp + i, 1) : StrL(" ");
        Rgba border = (i == active) ? ScInk() : ScBorder();
        cells->Child(Div(a)
                         ->W(28)
                         ->H(28)
                         ->ItemsCenter()
                         ->JustifyCenter()
                         ->Border(1, border)
                         ->Child(ScTxt(cx, StrDup(a, ch), 12, ScInk())));
    }
    return Div(a)
        ->FlexCol()
        ->W(224)
        ->Gap(4)
        ->Child(ScTxt(cx, StrL("Verification code"), 12, ScInk()))
        ->Child(cells)
        ->Child(ScTxt(cx, StrL("Enter the 6-digit code."), 12, ScMutedC()));
}

SHOWCASE_PAGE(CompOtpInput, ShowcaseOtpInput);
