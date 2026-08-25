#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

static void OnTextarea(ShowcaseApp* app, Ctx* cx, const ClickEvent*) {
    app->textareaOn = true;
    InputFocus(&app->textarea, cx);
    Notify(cx);
}

El* ShowcaseTextarea(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    return Div(a)
        ->FlexCol()
        ->W(224)
        ->Gap(4)
        ->ItemsStart()
        ->Child(Div(a)->H(16)->ItemsCenter()->Child(
            TextEl(a, StrL("Textarea"))->Font(12)->Fg(Rgb(0x17, 0x17, 0x17))))
        ->Child(InputBase::New(cx, StrL("example-textarea"), true)
                    ->OnClick(Listen(cx, &OnTextarea))
                    ->W(224)
                    ->H(64)
                    ->PadX(8)
                    ->PadY(8)
                    ->ClipY()
                    ->FocusId(0)
                    ->Border(1, app->textareaOn ? Rgb(0x17, 0x17, 0x17)
                                                : Rgb(0xd4, 0xd4, 0xd4))
                    ->Child(Textarea::New(cx, &app->textarea)));
}

SHOWCASE_PAGE(CompTextarea, ShowcaseTextarea);
