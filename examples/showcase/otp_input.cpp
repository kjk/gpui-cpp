#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

enum {
    ClickOtp = 410
};

El* ShowcaseOtpInput(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    int active = app->otpLen;
    if (active > 5) {
        active = 5;
    }
    El* cells = OtpInput::New(a, ClickOtp)->FlexRow()->Gap(4);
    for (int i = 0; i < 6; i++) {
        char ch[2] = {' ', 0};
        if (i < app->otpLen) {
            ch[0] = app->otp[i];
        }
        Rgba border = (i == active) ? ScInk() : ScBorder();
        cells->Child(Div(a)
                         ->W(28)
                         ->H(28)
                         ->ItemsCenter()
                         ->JustifyCenter()
                         ->Border(1, border)
                         ->Child(ScTxt(a, DupA(a, ch), 12, ScInk())));
    }
    return Div(a)
        ->FlexCol()
        ->W(224)
        ->Gap(4)
        ->Child(ScTxt(a, StrL("Verification code"), 12, ScInk()))
        ->Child(cells)
        ->Child(ScTxt(a, StrL("Enter the 6-digit code."), 12, ScMutedC()));
}

void ShowcaseOtpInputClick(ShowcaseApp* app, int id) {
    if (id == ClickOtp) {
        app->otpOn = true;
        app->input.focused = false;
    }
}

SHOWCASE_PAGE(CompOtpInput, ShowcaseOtpInput, ShowcaseOtpInputClick);
