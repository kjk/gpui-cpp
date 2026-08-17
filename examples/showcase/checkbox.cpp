#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

enum {
    ClickCheckbox = 280
};

El* ShowcaseCheckbox(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    bool on = app->checkboxOn;
    El* indicator = CheckboxIndicator::New(cx)
                        ->W(16)
                        ->H(16)
                        ->Shrink0()
                        ->ItemsCenter()
                        ->JustifyCenter()
                        ->Border(1, Rgb(0x17, 0x17, 0x17));
    if (on) {
        indicator->Bg(Rgb(0x17, 0x17, 0x17))
            ->Child(TextEl(a, StrL("✓"))->Font(11)->Fg(Rgb(0xff, 0xff, 0xff)));
    }
    return Checkbox::New(cx, StrL("example-checkbox"), ClickCheckbox)
        ->FlexRow()
        ->ItemsCenter()
        ->Gap(8)
        ->Child(indicator)
        ->Child(TextEl(a, StrL("Enable product updates"))
                    ->Font(12)
                    ->Fg(Rgb(0x17, 0x17, 0x17)));
}

void ShowcaseCheckboxClick(ShowcaseApp* app, int id) {
    if (id == ClickCheckbox) {
        app->checkboxOn = !app->checkboxOn;
    }
}

SHOWCASE_PAGE(CompCheckbox, ShowcaseCheckbox, ShowcaseCheckboxClick);
