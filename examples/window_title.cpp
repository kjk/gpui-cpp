#include "gpui.h"

static void OnInit(AppHost* host) {
    (void)host;
    ThemeSet(ThemeMode::Light);
}

static void OnClick(AppHost* host, int id) {
    (void)host;
    if (id == 1) {
        log("Clicked!");
    }
}

static El* OnRender(AppHost* host, Arena* frame, WinSize size) {
    (void)host;
    (void)size;
    const Theme& th = ThemeNow();
    El* bar = Div(frame)
                  ->FlexRow()
                  ->H(34)
                  ->PadX(12)
                  ->ItemsCenter()
                  ->JustifyBetween()
                  ->Bg(th.titleBar)
                  ->BorderT(0, th.titleBarBorder)
                  ->Child(TextEl(frame, StrL("App with Custom title bar"))
                              ->Font(14)
                              ->Fg(th.foreground))
                  ->Child(TextEl(frame, StrL("Right Item"))
                              ->Font(14)
                              ->Fg(th.mutedFg));

    El* body =
        Div(frame)
            ->FlexCol()
            ->Grow()
            ->Pad(20)
            ->ItemsCenter()
            ->JustifyCenter()
            ->Gap(8)
            ->Child(TextEl(frame, StrL("Hello, World!"))
                        ->Font(16)
                        ->Fg(th.foreground))
            ->Child(ButtonEl(frame, 1, StrL("Let's Go!"), BtnKind::Primary));

    return Div(frame)
        ->FlexCol()
        ->SizeFull()
        ->Bg(th.background)
        ->Child(bar)
        ->Child(Div(frame)->H(1)->W(kFill)->Bg(th.titleBarBorder))
        ->Child(body);
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    AppHooks hooks = {};
    hooks.onInit = OnInit;
    hooks.onRender = OnRender;
    hooks.onClick = OnClick;
    return RunApp(L"Window Title", 800, 600, hooks, nullptr);
}
