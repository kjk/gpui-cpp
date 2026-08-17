#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

enum {
    ClickInput = 380
};

El* ShowcaseInput(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    return Div(a)
        ->FlexCol()
        ->W(224)
        ->Gap(4)
        ->ItemsStart()
        ->Child(Div(a)->H(16)->ItemsCenter()->Child(
            TextEl(a, StrL("Project name"))
                ->Font(12)
                ->Fg(Rgb(0x17, 0x17, 0x17))))
        ->Child(InputBase::New(a, StrL("example-input"), ClickInput)
                    ->W(224)
                    ->H(28)
                    ->PadX(8)
                    ->ItemsCenter()
                    ->FocusId(0)
                    ->Border(1, app->input.focused ? Rgb(0x17, 0x17, 0x17)
                                                   : Rgb(0xd4, 0xd4, 0xd4))
                    ->Child(Input::New(a, &app->input)));
}

void ShowcaseInputClick(ShowcaseApp* app, int id) {
    if (id == ClickInput) {
        app->input.focused = true;
    }
}

SHOWCASE_PAGE(CompInput, ShowcaseInput, ShowcaseInputClick);
