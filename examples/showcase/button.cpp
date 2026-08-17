#include "Showcase.h"
#include "gpui.h"

enum {
    ClickBtnSave = 220,
    ClickBtnCancel = 221
};

El* ShowcaseButton(ShowcaseApp* app, Arena* a) {
    (void)app;
    return Div(a)
        ->FlexRow()
        ->ItemsCenter()
        ->Gap(8)
        ->Child(Button::New(a, StrL("primary-button"), ClickBtnSave)
                    ->PadX(12)
                    ->H(28)
                    ->ItemsCenter()
                    ->Font(12)
                    ->Border(1, Rgb(0x17, 0x17, 0x17))
                    ->Bg(Rgb(0x17, 0x17, 0x17))
                    ->HoverBg(Rgb(0x40, 0x40, 0x40))
                    ->Child(TextEl(a, StrL("Save changes"))
                                ->Font(12)
                                ->Fg(Rgb(0xff, 0xff, 0xff))))
        ->Child(Button::New(a, StrL("secondary-button"), ClickBtnCancel)
                    ->PadX(12)
                    ->H(28)
                    ->ItemsCenter()
                    ->Font(12)
                    ->Border(1, Rgb(0xd4, 0xd4, 0xd4))
                    ->Bg(Rgb(0xff, 0xff, 0xff))
                    ->HoverBg(Rgb(0xf5, 0xf5, 0xf5))
                    ->Child(TextEl(a, StrL("Cancel"))
                                ->Font(12)
                                ->Fg(Rgb(0x17, 0x17, 0x17))));
}

void ShowcaseButtonClick(ShowcaseApp* app, int id) {
    (void)app;
    if (id == ClickBtnSave) {
        log("Save changes");
    } else if (id == ClickBtnCancel) {
        log("Cancel");
    }
}

SHOWCASE_PAGE(CompButton, ShowcaseButton, ShowcaseButtonClick);
