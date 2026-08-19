#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

enum {
    ClickResize = 470
};

static void OnResize(ShowcaseApp* app, Ctx* cx, const ClickEvent*) {
    (void)app;
    app->draggingResize = true;
    Notify(cx);
}

El* ShowcaseResizable(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    float left = app->resizeW;
    if (left < 116) {
        left = 116;
    }
    if (left > 210) {
        left = 210;
    }
    El* nav = ResizablePanel::New(cx)
                  ->W(left)
                  ->H(kFill)
                  ->Pad(8)
                  ->FlexCol()
                  ->Gap(4)
                  ->Child(TextEl(a, StrL("PROJECT"))
                              ->Font(12)
                              ->Fg(Rgb(0x73, 0x73, 0x73)));
    const char* items[] = {"Overview", "Components", "Settings"};
    for (int i = 0; i < 3; i++) {
        nav->Child(Div(a)->H(26)->PadX(8)->ItemsCenter()->Child(
            TextEl(a, Str(items[i]))->Font(12)->Fg(Rgb(0x17, 0x17, 0x17))));
    }
    El* split = Div(a)
                    ->W(4)
                    ->H(kFill)
                    ->OnClick(Listen(cx, &OnResize))
                    ->FocusId(ClickResize);
    split->Child(Div(a)->W(1)->H(kFill)->Bg(Rgb(0x17, 0x17, 0x17)));
    El* main =
        ResizablePanel::New(cx)
            ->Grow()
            ->H(kFill)
            ->Pad(8)
            ->FlexCol()
            ->Gap(8)
            ->Child(TextEl(a, StrL("Workspace"))
                        ->Font(12)
                        ->Fg(Rgb(0x17, 0x17, 0x17)))
            ->Child(TextEl(a, StrL("Drag the divider to resize navigation."))
                        ->Font(12)
                        ->Fg(Rgb(0x73, 0x73, 0x73))
                        ->Wrap()
                        ->MaxW(140));
    return Resizable::New(cx, StrL("example-resizable"))
        ->W(288)
        ->H(160)
        ->Border(1, Rgb(0x17, 0x17, 0x17))
        ->FlexRow()
        ->Child(nav)
        ->Child(split)
        ->Child(main);
}

void ShowcaseResizeDrag(ShowcaseApp* app, Window* win, float x, float y) {
    (void)y;
    if (!app->draggingResize) {
        return;
    }
    for (int i = 0; i < win->paint.hits.len; i++) {
        HitRect h = win->paint.hits[i];
        if (h.id == ClickResize) {
            // move left panel: new width is x relative to the 288 box
            // approximate: hit x is divider; delta from divider center
            float boxLeft = h.bounds.x - app->resizeW;
            float w = x - boxLeft;
            if (w < 116) {
                w = 116;
            }
            if (w > 210) {
                w = 210;
            }
            app->resizeW = w;
            return;
        }
    }
}

SHOWCASE_PAGE(CompResizable, ShowcaseResizable);
