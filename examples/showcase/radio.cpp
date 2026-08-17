#include "Showcase.h"
#include "ui/Radio.h"

enum { ClickRadioStd = 460, ClickRadioExpress = 461 };

static El* RadioDot(Arena* a, bool on) {
    El* outer = Div(a)
                    ->W(14)
                    ->H(14)
                    ->Shrink0()
                    ->ItemsCenter()
                    ->JustifyCenter()
                    ->Border(1, Rgb(0x17, 0x17, 0x17));
    if (on) {
        outer->Child(Div(a)->W(6)->H(6)->Bg(Rgb(0x17, 0x17, 0x17)));
    }
    return outer;
}

static El* RadioRow(Arena* a, Str id, int clickId, bool on, const char* title, const char* sub, bool disabled) {
    El* row = Radio::New(a, id, disabled ? 0 : clickId)->FlexRow()->ItemsStart()->Gap(8);
    row->Child(Div(a)->PadT(2)->Child(RadioDot(a, on)));
    Rgba titleC = disabled ? Rgb(0x73, 0x73, 0x73) : Rgb(0x17, 0x17, 0x17);
    row->Child(Div(a)
                   ->FlexCol()
                   ->Child(TextEl(a, Str(title))->Font(12)->Fg(titleC))
                   ->Child(TextEl(a, Str(sub))->Font(12)->Fg(Rgb(0x73, 0x73, 0x73))));
    return row;
}

El* ShowcaseRadio(ShowcaseApp* app, Arena* a) {
    return RadioRow(a, StrL("example-radio"), ClickRadioStd, app->radioSel == 0, "Standard",
                    "3–5 business days", false);
}

void ShowcaseRadioClick(ShowcaseApp* app, int id) {
    if (id == ClickRadioStd) {
        app->radioSel = 0;
    }
}

El* ShowcaseRadioGroup(ShowcaseApp* app, Arena* a) {
    return RadioGroup::New(a, StrL("example-radio-group"))
        ->W(224)
        ->FlexCol()
        ->Gap(8)
        ->Child(RadioRow(a, StrL("example-radio"), ClickRadioStd, app->radioSel == 0, "Standard",
                         "3–5 business days", false))
        ->Child(RadioRow(a, StrL("express-radio"), ClickRadioExpress, app->radioSel == 1, "Express",
                         "Next business day", false))
        ->Child(RadioRow(a, StrL("pickup-radio"), 0, false, "Local pickup", "Currently unavailable", true));
}

void ShowcaseRadioGroupClick(ShowcaseApp* app, int id) {
    if (id == ClickRadioStd) {
        app->radioSel = 0;
    } else if (id == ClickRadioExpress) {
        app->radioSel = 1;
    }
}

SHOWCASE_PAGE(CompRadio, ShowcaseRadio, ShowcaseRadioClick);
SHOWCASE_PAGE(CompRadioGroup, ShowcaseRadioGroup, ShowcaseRadioGroupClick);

