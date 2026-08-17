#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

enum {
    ClickToggleB = 560,
    ClickToggleI = 561,
    ClickToggleU = 562
};

static El* ToggleCell(Arena* a, Str id, int clickId, const char* label,
                      bool on) {
    El* b = Toggle::New(a, id, clickId)
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
    return ToggleCell(a, StrL("example-toggle"), ClickToggleB, "B",
                      app->toggleOn);
}

void ShowcaseToggleClick(ShowcaseApp* app, int id) {
    if (id == ClickToggleB) {
        app->toggleOn = !app->toggleOn;
    }
}

El* ShowcaseToggleGroup(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    bool italic = (app->toggleGroup & 1) != 0;
    bool under = (app->toggleGroup & 2) != 0;
    return ToggleGroup::New(a, StrL("example-toggle-group"))
        ->FlexRow()
        ->Child(ShowcaseToggle(app, cx))
        ->Child(ToggleCell(a, StrL("italic-toggle"), ClickToggleI, "I", italic))
        ->Child(
            ToggleCell(a, StrL("underline-toggle"), ClickToggleU, "U", under));
}

void ShowcaseToggleGroupClick(ShowcaseApp* app, int id) {
    ShowcaseToggleClick(app, id);
    if (id == ClickToggleI) {
        app->toggleGroup ^= 1;
    } else if (id == ClickToggleU) {
        app->toggleGroup ^= 2;
    }
}

SHOWCASE_PAGE(CompToggle, ShowcaseToggle, ShowcaseToggleClick);
SHOWCASE_PAGE(CompToggleGroup, ShowcaseToggleGroup, ShowcaseToggleGroupClick);
