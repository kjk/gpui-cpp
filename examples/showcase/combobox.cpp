#include "Showcase.h"
#include "ui/Combobox.h"
#include "ui/Input.h"
#include "ui/Popup.h"

enum {
    ClickCombo = 320,
    ClickComboQ = 321,
    ClickCombo0 = 322
};

static const char* kFwCombo[] = {"GPUI", "React", "SwiftUI", "Vue"};

static bool Matches(const char* label, const char* q) {
    if (!q || !q[0]) {
        return true;
    }
    char a[32] = {};
    char b[32] = {};
    strncpy_s(a, label, _TRUNCATE);
    strncpy_s(b, q, _TRUNCATE);
    _strlwr_s(a);
    _strlwr_s(b);
    return strstr(a, b) != nullptr;
}

El* ShowcaseCombobox(ShowcaseApp* app, Arena* a) {
    El* trigger =
        Div(a)
            ->Id(StrL("combobox-trigger"))
            ->W(224)
            ->H(28)
            ->PadX(8)
            ->ItemsCenter()
            ->JustifyBetween()
            ->Border(1, Rgb(0xd4, 0xd4, 0xd4))
            ->Bg(Rgb(0xff, 0xff, 0xff))
            ->Click(ClickCombo)
            ->FocusId(ClickCombo)
            ->HoverBg(Rgb(0xf5, 0xf5, 0xf5))
            ->Child(TextEl(a, Str(app->comboboxSel))
                        ->Font(12)
                        ->Fg(Rgb(0x17, 0x17, 0x17)))
            ->Child(TextEl(a, StrL("⌄"))->Font(12)->Fg(Rgb(0x73, 0x73, 0x73)));
    El* pop = nullptr;
    if (app->comboboxOpen) {
        pop = Div(a)
                  ->FlexCol()
                  ->W(224)
                  ->Pad(4)
                  ->Border(1, Rgb(0xd4, 0xd4, 0xd4))
                  ->Bg(Rgb(0xff, 0xff, 0xff));
        pop->Child(InputBase::New(a, StrL("combobox-search"), ClickComboQ)
                       ->W(kFill)
                       ->H(28)
                       ->PadX(8)
                       ->ItemsCenter()
                       ->Border(1, Rgb(0xe5, 0xe5, 0xe5))
                       ->Child(Input::New(a, &app->comboQuery)));
        El* list = Div(a)->FlexCol()->W(kFill)->PadT(4);
        for (int i = 0; i < 4; i++) {
            if (!Matches(kFwCombo[i], app->comboQuery.buf)) {
                continue;
            }
            list->Child(Div(a)
                            ->W(kFill)
                            ->H(28)
                            ->PadX(8)
                            ->ItemsCenter()
                            ->HoverBg(Rgb(0xf5, 0xf5, 0xf5))
                            ->Click(ClickCombo0 + i)
                            ->Child(TextEl(a, Str(kFwCombo[i]))
                                        ->Font(12)
                                        ->Fg(Rgb(0x17, 0x17, 0x17))));
        }
        pop->Child(list);
    }
    El* combo =
        Combobox::New(a, StrL("example-combobox"))->W(224)->Child(trigger);
    return Popup::New(a, StrL("example-combobox-popup"), combo)
        ->Content(pop)
        ->IntoEl();
}

void ShowcaseComboboxClick(ShowcaseApp* app, int id) {
    if (id == ClickCombo) {
        app->comboboxOpen = !app->comboboxOpen;
        app->comboQuery.focused = app->comboboxOpen;
        app->input.focused = false;
    } else if (id == ClickComboQ) {
        app->comboQuery.focused = true;
        app->input.focused = false;
    } else if (id >= ClickCombo0 && id < ClickCombo0 + 4) {
        strncpy_s(app->comboboxSel, kFwCombo[id - ClickCombo0], _TRUNCATE);
        app->comboboxOpen = false;
        app->comboQuery.focused = false;
    }
}

SHOWCASE_PAGE(CompCombobox, ShowcaseCombobox, ShowcaseComboboxClick);
