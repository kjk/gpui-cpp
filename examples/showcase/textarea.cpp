#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

enum {
    ClickTextarea = 540
};

El* ShowcaseTextarea(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    return Div(a)
        ->FlexCol()
        ->W(224)
        ->Gap(4)
        ->ItemsStart()
        ->Child(Div(a)->H(16)->ItemsCenter()->Child(
            TextEl(a, StrL("Textarea"))->Font(12)->Fg(Rgb(0x17, 0x17, 0x17))))
        ->Child(InputBase::New(a, StrL("example-textarea"), ClickTextarea)
                    ->W(224)
                    ->H(64)
                    ->PadX(8)
                    ->PadY(8)
                    ->ClipY()
                    ->FocusId(0)
                    ->Border(1, app->textareaOn ? Rgb(0x17, 0x17, 0x17)
                                                : Rgb(0xd4, 0xd4, 0xd4))
                    ->Child(Textarea::New(
                        a, app->textarea,
                        app->textareaOn && ((GetTickCount() / 500) % 2 == 0))));
}

void ShowcaseTextareaClick(ShowcaseApp* app, int id) {
    if (id == ClickTextarea) {
        app->textareaOn = true;
        app->input.focused = false;
    }
}

SHOWCASE_PAGE(CompTextarea, ShowcaseTextarea, ShowcaseTextareaClick);
