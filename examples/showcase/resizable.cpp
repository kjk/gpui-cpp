#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

enum {
    ClickResize = 470
};

// The divider's own press, moves and release. GPUI's `div().on_mouse_down`,
// `on_drag_move` and `on_mouse_up`: the element that took the press keeps the
// moves until the button comes back up, so nothing here consults the window's
// pointer or walks last frame's hit rects to find itself again.
static void OnResizeDown(ShowcaseApp* app, Ctx* cx, const MouseDownEvent* ev) {
    if (ev->button != MouseButton::Left) {
        return;
    }
    app->draggingResize = true;
    Notify(cx);
}

// The width the divider has been dragged to. `ev->el` is the divider's own box
// as the last frame laid it out, so the panel's left edge is that less the
// width it had — and a handler that moves the divider reads its own answer
// back on the next move.
static void OnResizeDrag(ShowcaseApp* app, Ctx* cx, const DragMoveEvent* ev) {
    if (!app->draggingResize) {
        return;
    }
    float boxLeft = ev->el.x - app->resizeW;
    float w = ev->event.x - boxLeft;
    if (w < 116) {
        w = 116;
    }
    if (w > 210) {
        w = 210;
    }
    if (w == app->resizeW) {
        return;
    }
    app->resizeW = w;
    Notify(cx);
}

static void OnResizeUp(ShowcaseApp* app, Ctx* cx, const MouseUpEvent*) {
    if (!app->draggingResize) {
        return;
    }
    app->draggingResize = false;
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
                    ->Click(ClickResize)
                    ->Cursor(CursorKind::ColResize)
                    ->OnMouseDown(Listen(cx, &OnResizeDown))
                    ->OnDragMove(Listen(cx, &OnResizeDrag))
                    ->OnMouseUp(Listen(cx, &OnResizeUp))
                    ->FocusId(ClickResize);
    split->Child(Div(a)->W(1)->H(kFill)->Bg(Rgb(0x17, 0x17, 0x17)));
    El* main =
        ResizablePanel::New(cx)
            ->Flex1()
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

SHOWCASE_PAGE(CompResizable, ShowcaseResizable);
