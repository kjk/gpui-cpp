#include "gpui.h"

static void OnInit(AppHost* host) {
    (void)host;
    ThemeSet(ThemeMode::Light);
}

static El* Chip(Arena* a, Str s) {
    const Theme& th = ThemeNow();
    return Div(a)
        ->Radius(6)
        ->Border(1, th.border)
        ->PadX(12)
        ->PadY(8)
        ->Child(TextEl(a, s)->Font(14)->Fg(th.foreground));
}

static El* OnRender(AppHost* host, Arena* frame, WinSize size) {
    (void)host;
    (void)size;
    const Theme& th = ThemeNow();
    return Div(frame)
        ->FlexCol()
        ->SizeFull()
        ->Gap(16)
        ->Pad(32)
        ->Bg(th.background)
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
                    ->Child(Chip(frame, StrL("Root.bordered = false")))
                    ->Child(Chip(frame, StrL("window_decorations = Client"))));
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    AppHooks hooks = {};
    hooks.onInit = OnInit;
    hooks.onRender = OnRender;
    AppWinOpts opts = {};
    opts.borderless = true;
    return RunAppEx(L"Root Borderless", 640, 320, hooks, nullptr, opts);
}
