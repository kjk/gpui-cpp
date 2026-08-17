#include "gpui/Gpui.h"

static void OnInit(AppHost* host) {
    (void)host;
    ThemeSet(ThemeMode::Light);
}

static void OnClick(AppHost* host, int id) {
    (void)host;
    logf("button %d clicked", id);
}

static El* OnRender(AppHost* host, Arena* frame, WinSize size) {
    (void)host;
    (void)size;
    const Theme& th = ThemeNow();

    auto trapBtn = [&](int id, const char* label, int trap) {
        El* b = ButtonEl(frame, id, Str(label), BtnKind::Default);
        b->TrapId(trap);
        return b;
    };

    El* trap1 = Div(frame)
                    ->FlexRow()
                    ->Gap(8)
                    ->Pad(16)
                    ->Radius(th.radius)
                    ->Bg(th.secondary)
                    ->Border(1, th.border)
                    ->Child(trapBtn(11, "Trap 1 - Button 1", 1))
                    ->Child(trapBtn(12, "Trap 1 - Button 2", 1))
                    ->Child(trapBtn(13, "Trap 1 - Button 3", 1));

    El* trap2 = Div(frame)
                    ->FlexRow()
                    ->Gap(8)
                    ->Pad(16)
                    ->Radius(th.radius)
                    ->Bg(RgbaOpacity(th.accent, 0.4f))
                    ->Border(1, th.blue)
                    ->Child(trapBtn(21, "Trap 2 - Button 1", 2))
                    ->Child(trapBtn(22, "Trap 2 - Button 2", 2))
                    ->Child(trapBtn(23, "Trap 2 - Button 3", 2))
                    ->Child(trapBtn(24, "Trap 2 - Button 4", 2));

    return Div(frame)
        ->FlexCol()
        ->SizeFull()
        ->Gap(24)
        ->Pad(32)
        ->Bg(th.background)
        ->Child(TextEl(frame, StrL("Focus Trap Example"))->Font(20)->Bold()->Fg(th.foreground))
        ->Child(TextEl(frame, StrL("Press Tab to navigate between buttons. Notice how focus cycles within different areas."))
                    ->Font(14)
                    ->Fg(th.mutedFg)
                    ->Wrap())
        ->Child(TextEl(frame, StrL("Outside Area (No Focus Trap)"))->Font(16)->Semibold()->Fg(th.foreground))
        ->Child(Div(frame)
                    ->FlexRow()
                    ->Gap(8)
                    ->Child(ButtonEl(frame, 1, StrL("Outside Button 1")))
                    ->Child(ButtonEl(frame, 2, StrL("Outside Button 2")))
                    ->Child(ButtonEl(frame, 3, StrL("Outside Button 3"))))
        ->Child(TextEl(frame, StrL("Focus Trap Area 1"))->Font(16)->Semibold()->Fg(th.foreground))
        ->Child(trap1)
        ->Child(TextEl(frame, StrL("-> Press Tab in this area, focus cycles through 3 buttons without escaping"))
                    ->Font(12)
                    ->Fg(th.mutedFg))
        ->Child(TextEl(frame, StrL("Outside Area (No Focus Trap)"))->Font(16)->Semibold()->Fg(th.foreground))
        ->Child(Div(frame)
                    ->FlexRow()
                    ->Gap(8)
                    ->Child(ButtonEl(frame, 4, StrL("Outside Button 4")))
                    ->Child(ButtonEl(frame, 5, StrL("Outside Button 5"))))
        ->Child(TextEl(frame, StrL("Focus Trap Area 2"))->Font(16)->Semibold()->Fg(th.foreground))
        ->Child(trap2)
        ->Child(TextEl(frame, StrL("-> Press Tab in this area, focus cycles through 4 buttons without escaping"))
                    ->Font(12)
                    ->Fg(th.mutedFg));
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    AppHooks hooks = {};
    hooks.onInit = OnInit;
    hooks.onRender = OnRender;
    hooks.onClick = OnClick;
    return RunApp(L"Focus Trap", 800, 600, hooks, nullptr);
}
