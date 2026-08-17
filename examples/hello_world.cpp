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
    return Div(frame)
        ->FlexCol()
        ->SizeFull()
        ->Gap(8)
        ->ItemsCenter()
        ->JustifyCenter()
        ->Bg(th.background)
        ->Child(
            TextEl(frame, StrL("Hello, World!"))->Font(16)->Fg(th.foreground))
        ->Child(ButtonEl(frame, 1, StrL("Let's Go!"), BtnKind::Primary));
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    AppHooks hooks = {};
    hooks.onInit = OnInit;
    hooks.onRender = OnRender;
    hooks.onClick = OnClick;
    return RunApp(L"Hello World", 800, 600, hooks, nullptr);
}
