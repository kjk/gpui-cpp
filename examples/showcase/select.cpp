#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

enum {
    ClickSelect = 480,
    ClickSel0 = 481
};

static const char* kFw[] = {"GPUI", "React", "SwiftUI", "Vue"};

El* ShowcaseSelect(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    int sel = app->selectIx;
    if (sel < 0 || sel > 3) {
        sel = 0;
    }
    El* trigger = Div(a)
                      ->Id(StrL("select-trigger"))
                      ->H(28)
                      ->PadX(8)
                      ->ItemsCenter()
                      ->JustifyBetween()
                      ->Border(1, Rgb(0x17, 0x17, 0x17))
                      ->Bg(Rgb(0xff, 0xff, 0xff))
                      ->Click(ClickSelect)
                      ->FocusId(0)
                      ->HoverBg(Rgb(0xf5, 0xf5, 0xf5))
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
                          ->PadX(8)
                          ->PadY(4)
                          ->FlexRow()
                          ->JustifyBetween()
                          ->HoverBg(Rgb(0xf5, 0xf5, 0xf5))
                          ->Click(ClickSel0 + i)
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
    El* root = Select::New(a, StrL("example-select"))->W(224)->Child(trigger);
    return Popup::New(a, StrL("example-select-options"), root)
        ->Content(opts)
        ->IntoEl();
}

void ShowcaseSelectClick(ShowcaseApp* app, int id) {
    if (id == ClickSelect) {
        app->selectOpen = !app->selectOpen;
    } else if (id >= ClickSel0 && id < ClickSel0 + 4) {
        app->selectIx = id - ClickSel0;
        app->selectOpen = false;
    }
}

SHOWCASE_PAGE(CompSelect, ShowcaseSelect, ShowcaseSelectClick);
