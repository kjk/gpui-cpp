#include "gpui.h"

using namespace gpui;

struct InputApp {
    LineInput in;
    char display[256];
};

static void OnInit(AppHost* host) {
    ThemeSet(ThemeMode::Light);
    auto* app = (InputApp*)host->user;
    strncpy_s(app->in.placeholder, "Enter your name", _TRUNCATE);
    app->display[0] = 0;
    host->input = &app->in;
    app->in.focused = true;
}

static El* InputBox(Arena* a, LineInput* in, const Theme& th) {
    Str shown = in->len > 0 ? Str(in->buf, in->len) : Str(in->placeholder);
    Rgba fg = in->len > 0 ? th.foreground : th.mutedFg;
    return Div(a)
        ->W(320)
        ->H(36)
        ->PadX(12)
        ->ItemsCenter()
        ->Radius(6)
        ->Border(1, th.border)
        ->Bg(th.background)
        ->Child(TextEl(a, shown)->Font(14)->Fg(fg));
}

static El* OnRender(AppHost* host, Arena* frame, WinSize size) {
    (void)size;
    auto* app = (InputApp*)host->user;
    const Theme& th = ThemeNow();
    El* col = Div(frame)
                  ->FlexCol()
                  ->SizeFull()
                  ->Pad(20)
                  ->Gap(8)
                  ->ItemsCenter()
                  ->JustifyCenter()
                  ->Bg(th.background);
    col->Child(InputBox(frame, &app->in, th));
    if (app->display[0]) {
        col->Child(
            TextEl(frame, Str(app->display))->Font(16)->Fg(th.foreground));
    }
    return col;
}

static void OnChar(AppHost* host, uint32_t cp) {
    (void)cp;
    auto* app = (InputApp*)host->user;
    if (app->in.len > 0) {
        _snprintf_s(app->display, _TRUNCATE, "Hello, %s!", app->in.buf);
    } else {
        app->display[0] = 0;
    }
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    static InputApp app;
    AppHooks hooks = {};
    hooks.onInit = OnInit;
    hooks.onRender = OnRender;
    hooks.onChar = OnChar;
    return RunApp(L"Input", 800, 600, hooks, &app);
}
