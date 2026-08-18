#include "gpui.h"

using namespace gpui;

// examples/window_title — a client-drawn title strip above the body.
struct Example {
    static void OnGo(Example*, Ctx*, const ClickEvent*) {
        log(StrL("Clicked!"));
    }

    static El* Render(Example*, Ctx* cx) {
        Arena* a = cx->a;
        const Theme& th = cx->theme();
        El* bar = Div(a)
                      ->FlexRow()
                      ->H(34)
                      ->PadX(12)
                      ->ItemsCenter()
                      ->JustifyBetween()
                      ->Bg(th.titleBar)
                      ->BorderT(0, th.titleBarBorder)
                      ->Child(TextEl(a, StrL("App with Custom title bar"))
                                  ->Font(14)
                                  ->Fg(th.foreground))
                      ->Child(TextEl(a, StrL("Right Item"))
                                  ->Font(14)
                                  ->Fg(th.mutedFg));

        El* body =
            Div(a)
                ->FlexCol()
                ->Grow()
                ->Pad(20)
                ->ItemsCenter()
                ->JustifyCenter()
                ->Gap(8)
                ->Child(TextEl(a, StrL("Hello, World!"))
                            ->Font(16)
                            ->Fg(th.foreground))
                ->Child(ButtonEl(a, 0, StrL("Let's Go!"), BtnKind::Primary)
                            ->OnClick(Listen(cx, &Example::OnGo)));

        return Div(a)
            ->FlexCol()
            ->SizeFull()
            ->Bg(th.background)
            ->Child(bar)
            ->Child(Div(a)->H(1)->W(kFill)->Bg(th.titleBarBorder))
            ->Child(body);
    }
};

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    App* app = AppNew();
    ThemeSet(app, ThemeMode::Light);
    return AppRunView(StrL("Window Title"), 800, 600,
                      EntityNew<Example>(app).id, app, WinOpts{});
}
