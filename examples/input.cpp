#include "gpui.h"

using namespace gpui;

// examples/input — an Input bound to its state. The view subscribes to the
// state's change event and republishes the greeting from the value.
struct Example {
    LineInput inputState;
    char displayText[560] = {};
    bool subscribed = false;

    static void OnChange(Example* self, Ctx* cx, const InputEvent*) {
        snprintf(self->displayText, sizeof(self->displayText), "Hello, %s!",
                 self->inputState.buf);
        Notify(cx);
    }

    // Clicking the field focuses it; GPUI routes that through the focus
    // handle the Input owns.
    static void OnFocus(Example* self, Ctx* cx, const ClickEvent*) {
        self->inputState.focused = true;
        Notify(cx);
    }

    static El* Render(Example* self, Ctx* cx) {
        Arena* a = cx->a;
        const Theme& th = cx->theme();
        if (!self->subscribed) {
            self->subscribed = true;
            self->inputState.onChange =
                ListenTo(Entity<Example>{cx->self}, &Example::OnChange);
        }
        // The window routes WM_CHAR into whichever LineInput has focus.
        if (self->inputState.focused) {
            cx->win->input = &self->inputState;
        }
        return Div(a)
            ->FlexCol()
            ->Pad(20)
            ->Gap(8)
            ->SizeFull()
            ->ItemsCenter()
            ->JustifyCenter()
            ->Bg(th.background)
            ->Child(component::Input::New(cx, StrL("input"), &self->inputState)
                        ->OnFocus(Listen(cx, &Example::OnFocus))
                        ->IntoEl())
            ->Child(TextEl(a, Str(self->displayText))->Fg(th.foreground));
    }
};

int GpuiMain(int argc, char** argv) {
    (void)argc;
    (void)argv;
    App* app = AppNew();
    ThemeSet(app, ThemeMode::Light);
    Entity<Example> view = EntityNew<Example>(app);
    Example* self = view.Get(app);
    StrCopyZ(self->inputState.placeholder,
             (int)sizeof(self->inputState.placeholder), "Enter your name");

    return AppRunView(StrL("Input"), 800, 600, view.id, app, WinOpts{});
}
