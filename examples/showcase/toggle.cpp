#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

enum {
    ClickToggleB = 560,
    ClickToggleI = 561,
    ClickToggleU = 562
};

static void ToggleBold(ShowcaseApp* app, Ctx* cx, const ClickEvent*) {
    app->toggleOn = !app->toggleOn;
    Notify(cx);
}

static El* ToggleCell(Ctx* cx, Str id, Listener onClick, const char* label,
                      bool on) {
    Arena* a = cx->a;
    El* b = Toggle::New(cx, id, 0)
                ->OnClick(onClick)
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
    Arena* a = cx->a;
    return ToggleCell(cx, StrL("example-toggle"), Listen(cx, &ToggleBold), "B",
                      app->toggleOn);
}

void ShowcaseToggleClick(ShowcaseApp* app, int id) {
    (void)app;
    (void)id;
}

El* ShowcaseToggleGroup(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    bool italic = (app->toggleGroup & 1) != 0;
    bool under = (app->toggleGroup & 2) != 0;
    return ToggleGroup::New(cx, StrL("example-toggle-group"))
        ->FlexRow()
        ->Child(ShowcaseToggle(app, cx))
        ->Child(ToggleCell(cx, StrL("italic-toggle"), Listener{}, "I", italic))
        ->Child(
            ToggleCell(cx, StrL("underline-toggle"), Listener{}, "U", under));
}

void ShowcaseToggleGroupClick(ShowcaseApp* app, int id) {
    (void)app;
    (void)id;
}

SHOWCASE_PAGE(CompToggle, ShowcaseToggle, ShowcaseToggleClick);
SHOWCASE_PAGE(CompToggleGroup, ShowcaseToggleGroup, ShowcaseToggleGroupClick);
