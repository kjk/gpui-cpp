#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

enum {
    ClickRadioStd = 460,
    ClickRadioExpress = 461
};

static void PickRadio(ShowcaseApp* app, Ctx* cx, const ClickEvent*,
                      intptr_t ix) {
    app->radioSel = (int)ix;
    Notify(cx);
}

static El* RadioDot(Ctx* cx, bool on) {
    Arena* a = cx->a;
    El* outer =
        Div(a)->W(14)->H(14)->Shrink0()->ItemsCenter()->JustifyCenter()->Border(
            1, Rgb(0x17, 0x17, 0x17));
    if (on) {
        outer->Child(Div(a)->W(6)->H(6)->Bg(Rgb(0x17, 0x17, 0x17)));
    }
    return outer;
}

static El* RadioRow(Ctx* cx, Str id, Listener onClick, bool on,
                    const char* title, const char* sub, bool disabled) {
    Arena* a = cx->a;
    El* row = Radio::New(cx, id, on, disabled, onClick)
                  ->FlexRow()
                  ->ItemsStart()
                  ->Gap(8);
    row->Child(Div(a)->PadT(2)->Child(RadioDot(cx, on)));
    Rgba titleC = disabled ? Rgb(0x73, 0x73, 0x73) : Rgb(0x17, 0x17, 0x17);
    row->Child(
        Div(a)
            ->FlexCol()
            ->Child(TextEl(a, Str(title))->Font(12)->Fg(titleC))
            ->Child(TextEl(a, Str(sub))->Font(12)->Fg(Rgb(0x73, 0x73, 0x73))));
    return row;
}

El* ShowcaseRadio(ShowcaseApp* app, Ctx* cx) {
    return RadioRow(cx, StrL("example-radio"), Listen(cx, &PickRadio, 0),
                    app->radioSel == 0, "Standard", "3–5 business days", false);
}

El* ShowcaseRadioGroup(ShowcaseApp* app, Ctx* cx) {
    return RadioGroup::New(cx, StrL("example-radio-group"))
        ->W(224)
        ->FlexCol()
        ->Gap(8)
        ->Child(RadioRow(cx, StrL("example-radio"), Listen(cx, &PickRadio, 0),
                         app->radioSel == 0, "Standard", "3–5 business days",
                         false))
        ->Child(RadioRow(cx, StrL("express-radio"), Listen(cx, &PickRadio, 1),
                         app->radioSel == 1, "Express", "Next business day",
                         false))
        ->Child(RadioRow(cx, StrL("pickup-radio"), Listener{}, false,
                         "Local pickup", "Currently unavailable", true));
}

SHOWCASE_PAGE(CompRadio, ShowcaseRadio);
SHOWCASE_PAGE(CompRadioGroup, ShowcaseRadioGroup);
