#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

static const char* kFw[] = {"GPUI", "React", "SwiftUI", "Vue"};

static void ToggleSelect(ShowcaseApp* app, Ctx* cx, const ClickEvent*) {
    app->selectOpen = !app->selectOpen;
    Notify(cx);
}

static void PickOption(ShowcaseApp* app, Ctx* cx, const ClickEvent*,
                       intptr_t ix) {
    app->selectIx = (int)ix;
    app->selectOpen = false;
    Notify(cx);
}

El* ShowcaseSelect(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    int sel = app->selectIx;
    if (sel < 0 || sel > 3) {
        sel = 0;
    }
    // The trigger fills the select root: GPUI lays a plain div out as a block,
    // so its child spans the w_56 the root was given.
    El* trigger = Div(a)
                      ->Id(StrL("select-trigger"))
                      ->Click(HashClickId(StrL("select-trigger")))
                      ->W(kFill)
                      ->H(28)
                      ->PadX(8)
                      ->ItemsCenter()
                      ->JustifyBetween()
                      ->Border(1, Rgb(0x17, 0x17, 0x17))
                      ->OnClick(Listen(cx, &ToggleSelect))
                      ->Child(TextEl(a, Str(kFw[sel]))
                                  ->Font(12)
                                  ->Fg(Rgb(0x17, 0x17, 0x17)))
                      ->Child(TextEl(a, app->selectOpen ? StrL("⌃") : StrL("⌄"))
                                  ->Font(12)
                                  ->Fg(Rgb(0x17, 0x17, 0x17)));
    El* opts = nullptr;
    if (app->selectOpen) {
        opts = Div(a)
                   ->FlexCol()
                   ->Pad(4)
                   ->Border(1, Rgb(0x17, 0x17, 0x17))
                   ->Bg(Rgb(0xff, 0xff, 0xff));
        for (int i = 0; i < 4; i++) {
            El* row = Div(a)
                          ->Click(HashClickId(fmt("select-option-%d", i)))
                          ->PadX(8)
                          ->PadY(4)
                          ->FlexRow()
                          ->JustifyBetween()
                          ->HoverBg(Rgb(0xf5, 0xf5, 0xf5))
                          ->OnClick(Listen(cx, &PickOption, i))
                          ->Child(TextEl(a, Str(kFw[i]))
                                      ->Font(12)
                                      ->Fg(Rgb(0x17, 0x17, 0x17)));
            if (i == sel) {
                row->Child(
                    TextEl(a, StrL("✓"))->Font(12)->Fg(Rgb(0x17, 0x17, 0x17)));
            }
            opts->Child(row);
        }
    }
    El* root = Select::New(cx, StrL("example-select"))->W(224)->Child(trigger);
    return Popup::New(cx, StrL("example-select-options"), root)
        ->Content(opts)
        ->IntoEl();
}

SHOWCASE_PAGE(CompSelect, ShowcaseSelect);
