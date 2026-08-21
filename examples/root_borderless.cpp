#include "gpui.h"

using namespace gpui;

static El* Chip(Ctx* cx, Str s) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    return Div(a)
        ->Radius(6)
        ->Border(1, th.border)
        ->PadX(12)
        ->PadY(8)
        ->Child(TextEl(a, s)->Font(14)->Fg(th.foreground));
}

// examples/root_borderless — Root::bordered(false) with client decorations.
struct Example {
    static El* Render(Example*, Ctx* cx);
};

El* Example::Render(Example*, Ctx* cx) {
    Arena* frame = cx->a;
    const Theme& th = cx->theme();
    return Div(frame)
        ->FlexCol()
        ->SizeFull()
        ->Gap(16)
        ->Pad(32)
        ->Bg(th.tokens.background)
        ->Child(TextEl(frame, StrL("Root::bordered(false)"))
                    ->Font(24)
                    ->Semibold()
                    ->Fg(th.foreground))
        ->Child(TextEl(frame, StrL("This window requests client-side "
                                   "decorations, while Root disables "
                                   "GPUI Component's window border wrapper."))
                    ->Font(14)
                    ->Fg(th.mutedFg)
                    ->MaxW(560)
                    ->Wrap())
        ->Child(Div(frame)
                    ->FlexRow()
                    ->Gap(12)
                    ->Child(Chip(cx, StrL("Root.bordered = false")))
                    ->Child(Chip(cx, StrL("window_decorations = Client"))));
}

int GpuiMain(int argc, char** argv) {
    (void)argc;
    (void)argv;
    App* app = AppNew();
    ThemeSet(app, ThemeMode::Light);
    WinOpts opts = {};
    opts.borderless = true;
    return AppRunView(StrL("Root Borderless C++"), 640, 320,
                      EntityNew<Example>(app).id, app, opts);
}
