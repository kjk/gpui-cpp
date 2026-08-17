#include "gpui.h"

static void OnInit(AppHost* host) {
    (void)host;
    ThemeSet(ThemeMode::Light);
}

static El* OnRender(AppHost* host, Arena* frame, WinSize size) {
    (void)host;
    (void)size;
    const Theme& th = ThemeNow();
    El* btn = ButtonEl(frame, 1, StrL("Hover for tooltip"), BtnKind::Primary);
    btn->Tip(StrL(
        "This tooltip should appear below the trigger near the top edge."));

    return Div(frame)
        ->SizeFull()
        ->Bg(th.background)
        ->Child(Div(frame)->Absolute()->Top(0)->Left(24)->Child(btn))
        ->Child(Div(frame)->Absolute()->Top(64)->Left(24)->MaxW(420)->Child(
            TextEl(
                frame,
                StrL("Hover the top button. The tooltip should flip below the "
                     "trigger without changing the original visual gap."))
                ->Font(14)
                ->Fg(th.mutedFg)
                ->Wrap()
                ->MaxW(420)));
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    AppHooks hooks = {};
    hooks.onInit = OnInit;
    hooks.onRender = OnRender;
    return RunApp(L"Tooltip Top Edge", 520, 260, hooks, nullptr);
}
