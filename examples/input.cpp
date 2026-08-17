#include "gpui.h"

using namespace gpui;

// examples/input — the view owns the field, and greets on every keystroke.
struct Example {
    LineInput in;
    char display[256] = {};

    static void OnKey(Example* self, Ctx* cx, const KeyEvent* ev) {
        if (!ev || ev->ch == 0) {
            return;
        }
        if (self->in.len > 0) {
            _snprintf_s(self->display, _TRUNCATE, "Hello, %s!", self->in.buf);
        } else {
            self->display[0] = 0;
        }
        Notify(cx);
    }

    static El* Field(Arena* a, LineInput* in, const Theme& th) {
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

    static El* Render(Example* self, Ctx* cx) {
        Arena* a = cx->a;
        const Theme& th = ThemeNow();
        // The window routes WM_CHAR into whichever LineInput has focus.
        cx->win->input = &self->in;
        El* col = Div(a)
                      ->FlexCol()
                      ->SizeFull()
                      ->Pad(20)
                      ->Gap(8)
                      ->ItemsCenter()
                      ->JustifyCenter()
                      ->Bg(th.background);
        col->Child(Field(a, &self->in, th));
        if (self->display[0]) {
            col->Child(
                TextEl(a, Str(self->display))->Font(16)->Fg(th.foreground));
        }
        return col;
    }
};

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    App* app = AppNew();
    ThemeSet(ThemeMode::Light);
    Entity<Example> view = EntityNew<Example>(app);
    Example* self = view.Get(app);
    strncpy_s(self->in.placeholder, "Enter your name", _TRUNCATE);
    self->in.focused = true;

    Window* win =
        WindowOpenView(app, L"Input", 800, 600, view.id, AppWinOpts{});
    WindowOnKey(win, ListenTo(view, &Example::OnKey));
    int rc = AppRun(app);
    AppFree(app);
    return rc;
}
