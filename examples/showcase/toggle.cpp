#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

// The toggle reports the value its activation produces, the way Rust's
// on_change hands the handler `!pressed`.
static void ToggleBold(ShowcaseApp* app, Ctx* cx, const ClickEvent*,
                       intptr_t next) {
    app->toggleOn = next != 0;
    Notify(cx);
}

static El* ToggleCell(Ctx* cx, Str id, Listener onChange, const char* label,
                      bool on) {
    Arena* a = cx->a;
    El* b = Toggle::New(cx, id, on, false, onChange)
                ->W(28)
                ->H(28)
                ->ItemsCenter()
                ->JustifyCenter()
                ->Border(1, Rgb(0x17, 0x17, 0x17));
    if (on) {
        b->Bg(Rgb(0x17, 0x17, 0x17))
            ->Child(TextEl(a, Str(label))
                        ->Font(12)
                        ->Fg(Rgb(0xff, 0xff, 0xff))
                        ->Bold());
    } else {
        b->Bg(Rgb(0xff, 0xff, 0xff))
            ->HoverBg(Rgb(0xf5, 0xf5, 0xf5))
            ->Child(TextEl(a, Str(label))
                        ->Font(12)
                        ->Fg(Rgb(0x17, 0x17, 0x17))
                        ->Bold());
    }
    return b;
}

El* ShowcaseToggle(ShowcaseApp* app, Ctx* cx) {
    return ToggleCell(cx, StrL("example-toggle"), Listen(cx, &ToggleBold), "B",
                      app->toggleOn);
}

El* ShowcaseToggleGroup(ShowcaseApp* app, Ctx* cx) {
    bool italic = (app->toggleGroup & 1) != 0;
    bool under = (app->toggleGroup & 2) != 0;
    return ToggleGroup::New(cx, StrL("example-toggle-group"))
        ->FlexRow()
        ->Child(ShowcaseToggle(app, cx))
        ->Child(ToggleCell(cx, StrL("italic-toggle"), Listener{}, "I", italic))
        ->Child(
            ToggleCell(cx, StrL("underline-toggle"), Listener{}, "U", under));
}

SHOWCASE_PAGE(CompToggle, ShowcaseToggle);
SHOWCASE_PAGE(CompToggleGroup, ShowcaseToggleGroup);
