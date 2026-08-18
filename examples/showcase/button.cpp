#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

enum {
    ClickBtnSave = 220,
    ClickBtnCancel = 221
};

static void SaveClicked(ShowcaseApp*, Ctx*, const ClickEvent*) {
    log(StrL("Save changes"));
}

static void CancelClicked(ShowcaseApp*, Ctx*, const ClickEvent*) {
    log(StrL("Cancel"));
}

El* ShowcaseButton(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    (void)app;
    return Div(a)
        ->FlexRow()
        ->ItemsCenter()
        ->Gap(8)
        ->Child(Button::New(cx, StrL("primary-button"), 0)
                    ->OnClick(Listen(cx, &SaveClicked))
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
        ->Child(Button::New(cx, StrL("secondary-button"), 0)
                    ->OnClick(Listen(cx, &CancelClicked))
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
    (void)id;
}

SHOWCASE_PAGE(CompButton, ShowcaseButton, ShowcaseButtonClick);
