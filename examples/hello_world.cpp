#include "gpui.h"

using namespace gpui;

// examples/hello_world/src/main.rs — a stateless view with a click listener.
struct Example {
    static void OnGo(Example*, Ctx*, const ClickEvent*) {
        log(StrL("Clicked!"));
    }

    static El* Render(Example*, Ctx* cx) {
        const Theme& th = ThemeNow();
        return Div(cx->a)
            ->FlexCol()
            ->SizeFull()
            ->Gap(8)
            ->ItemsCenter()
            ->JustifyCenter()
            ->Bg(th.background)
            ->Child(TextEl(cx->a, StrL("Hello, World!"))
                        ->Font(16)
                        ->Fg(th.foreground))
            ->Child(ButtonEl(cx->a, 0, StrL("Let's Go!"), BtnKind::Primary)
                        ->OnClick(Listen(cx, &Example::OnGo)));
    }
};

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    App* app = AppNew();
    ThemeSet(ThemeMode::Light);
    return AppRunView(StrL("Hello World"), 800, 600, EntityNew<Example>(app).id,
                      app, WinOpts{});
}
