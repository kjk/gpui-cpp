#include "Story.h"

static component::Button* Btn(Arena* a, StoryApp* app, const char* id) {
    return component::Button::New(a, Str(id))->WithSize(app->size);
}

static El* ProgressIcon(Arena* a, float value, Rgba color, bool hasColor) {
    component::ProgressCircle* p =
        component::ProgressCircle::New(a)->Value(value)->Size(14)->Label(false);
    if (hasColor) {
        p->Color(color);
    }
    return p->IntoEl();
}

static El* BtnGroup(Arena* a, StoryApp* app, bool vertical,
                    const char* prefix) {
    const Theme& th = ThemeNow();
    const char* labels[] = {"One", "Two", "Three"};
    El* g = vertical ? Div(a)->FlexCol() : Div(a)->FlexRow();
    g->Border(1, th.border)->Radius(th.radius);
    for (int i = 0; i < 3; i++) {
        char id[32];
        _snprintf_s(id, _TRUNCATE, "%s-%d", prefix, i);
        El* b = Btn(a, app, id)->Label(Str(labels[i]))->IntoEl();
        if (!vertical && i < 2) {
            b->Border(0, th.border);
        }
        g->Child(b);
    }
    return g;
}

El* ButtonRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(a, app));

    El* vars = StorySection(a, "Variants",
                            "Visual treatments communicate action priority.");
    El* row = Div(a)->FlexRow()->Gap(8)->ItemsCenter()->Wrap();
    row->Child(Btn(a, app, "button-0")->Label(StrL("Default"))->IntoEl());
    row->Child(
        Btn(a, app, "button-1")->Label(StrL("Primary"))->Primary()->IntoEl());
    row->Child(Btn(a, app, "button-2")
                   ->Label(StrL("Secondary"))
                   ->Secondary()
                   ->IntoEl());
    row->Child(
        Btn(a, app, "button-4")->Label(StrL("Danger"))->Danger()->IntoEl());
    row->Child(Btn(a, app, "button-4-warning")
                   ->Label(StrL("Warning"))
                   ->Warning()
                   ->IntoEl());
    row->Child(Btn(a, app, "button-4-success")
                   ->Label(StrL("Success"))
                   ->Success()
                   ->IntoEl());
    row->Child(
        Btn(a, app, "button-5-info")->Label(StrL("Info"))->Info()->IntoEl());
    row->Child(
        Btn(a, app, "button-5-ghost")->Label(StrL("Ghost"))->Ghost()->IntoEl());
    row->Child(
        Btn(a, app, "button-5-link")->Label(StrL("Link"))->Link()->IntoEl());
    row->Child(
        Btn(a, app, "button-5-text")->Label(StrL("Text"))->Text()->IntoEl());
    StorySectionAdd(vars, row);
    page->Child(vars);

    El* icons = StorySection(
        a, "Icons", "Icons can lead labels or appear in custom content.");
    El* iconRow = Div(a)->FlexRow()->Gap(8)->ItemsCenter()->Wrap();
    iconRow->Child(Btn(a, app, "button-icon-1")
                       ->Outline()
                       ->Label(StrL("Confirm"))
                       ->Icon(IconName::Check)
                       ->IntoEl());
    iconRow->Child(Btn(a, app, "button-icon-2")
                       ->Outline()
                       ->Label(StrL("Abort"))
                       ->Icon(IconName::X)
                       ->IntoEl());
    iconRow->Child(Btn(a, app, "button-icon-3")
                       ->Outline()
                       ->Label(StrL("Maximize"))
                       ->Icon(IconName::Maximize)
                       ->IntoEl());
    El* custom = Div(a)->FlexRow()->ItemsCenter()->Gap(8);
    custom->Child(StoryTxt(a, StrL("Custom Child"), 14, th.foreground));
    custom->Child(IconEl(a, IconName::ChevronDown, 14)->Fg(th.foreground));
    custom->Child(IconEl(a, IconName::Eye, 14)->Fg(th.foreground));
    iconRow->Child(Btn(a, app, "button-icon-4")->Extra(custom)->IntoEl());
    iconRow->Child(Btn(a, app, "button-icon-5-ghost")
                       ->Ghost()
                       ->Icon(IconName::Check)
                       ->Label(StrL("Confirm"))
                       ->IntoEl());
    iconRow->Child(Btn(a, app, "button-icon-6-link")
                       ->Link()
                       ->Icon(IconName::Check)
                       ->Label(StrL("Link"))
                       ->IntoEl());
    iconRow->Child(Btn(a, app, "button-icon-6-text")
                       ->Text()
                       ->Icon(IconName::Check)
                       ->Label(StrL("Text Button"))
                       ->IntoEl());
    StorySectionAdd(icons, iconRow);
    page->Child(icons);

    El* prog =
        StorySection(a, "Progress", "Buttons can show determinate progress.");
    El* progRow = Div(a)->FlexRow()->Gap(16)->ItemsCenter()->Wrap();
    progRow->Child(Btn(a, app, "progress-button-1")
                       ->Primary()
                       ->Extra(ProgressIcon(a, 25, th.primaryFg, true))
                       ->Label(StrL("Installing..."))
                       ->IntoEl());
    progRow->Child(Btn(a, app, "progress-button-2")
                       ->Extra(ProgressIcon(a, 35, {}, false))
                       ->Label(StrL("Installing..."))
                       ->IntoEl());
    progRow->Child(Btn(a, app, "progress-button-3")
                       ->Extra(ProgressIcon(a, 68, {}, false))
                       ->Label(StrL("Installing..."))
                       ->IntoEl());
    progRow->Child(Btn(a, app, "progress-button-4")
                       ->Extra(ProgressIcon(a, 85, {}, false))
                       ->Label(StrL("Installing..."))
                       ->IntoEl());
    StorySectionAdd(prog, progRow);
    page->Child(prog);

    El* out = StorySection(a, "Outline",
                           "Outlined treatments keep actions visually quiet.");
    El* outRow = Div(a)->FlexRow()->Gap(8)->ItemsCenter()->Wrap();
    outRow->Child(Btn(a, app, "button-outline-1")
                      ->Primary()
                      ->Outline()
                      ->Label(StrL("Primary Button"))
                      ->IntoEl());
    outRow->Child(Btn(a, app, "button-outline-2")
                      ->Outline()
                      ->Label(StrL("Normal Button"))
                      ->IntoEl());
    outRow->Child(Btn(a, app, "button-outline-4-danger")
                      ->Danger()
                      ->Outline()
                      ->Label(StrL("Danger Button"))
                      ->IntoEl());
    outRow->Child(Btn(a, app, "button-outline-4-warning")
                      ->Warning()
                      ->Outline()
                      ->Label(StrL("Warning Button"))
                      ->IntoEl());
    outRow->Child(Btn(a, app, "button-outline-4-success")
                      ->Success()
                      ->Outline()
                      ->Label(StrL("Success Button"))
                      ->IntoEl());
    outRow->Child(Btn(a, app, "button-outline-5-info")
                      ->Info()
                      ->Outline()
                      ->Label(StrL("Info Button"))
                      ->IntoEl());
    outRow->Child(Btn(a, app, "button-outline-5-ghost")
                      ->Ghost()
                      ->Outline()
                      ->Label(StrL("Ghost Button"))
                      ->IntoEl());
    outRow->Child(Btn(a, app, "button-outline-5-link")
                      ->Link()
                      ->Outline()
                      ->Label(StrL("Link Button"))
                      ->IntoEl());
    outRow->Child(Btn(a, app, "button-outline-5-text")
                      ->Text()
                      ->Outline()
                      ->Label(StrL("Text Button"))
                      ->IntoEl());
    StorySectionAdd(out, outRow);
    page->Child(out);

    El* drop =
        StorySection(a, "Dropdown", "A caret indicates an attached menu.");
    El* dropRow = Div(a)->FlexRow()->Gap(8)->ItemsCenter()->Wrap();
    dropRow->Child(Btn(a, app, "button-dropdown-caret-primary")
                       ->Primary()
                       ->DropdownCaret()
                       ->Label(StrL("Primary Button"))
                       ->IntoEl());
    dropRow->Child(Btn(a, app, "button-dropdown-caret-default")
                       ->DropdownCaret()
                       ->Label(StrL("Default Button"))
                       ->IntoEl());
    dropRow->Child(Btn(a, app, "button-outline-3")
                       ->Secondary()
                       ->DropdownCaret()
                       ->Label(StrL("Secondary Button"))
                       ->IntoEl());
    dropRow->Child(Btn(a, app, "button-dropdown-caret-ghost")
                       ->Ghost()
                       ->DropdownCaret()
                       ->Label(StrL("Ghost Button"))
                       ->IntoEl());
    dropRow->Child(Btn(a, app, "button-dropdown-caret-link")
                       ->Link()
                       ->DropdownCaret()
                       ->Label(StrL("Link Button"))
                       ->IntoEl());
    dropRow->Child(Btn(a, app, "button-dropdown-caret-small")
                       ->Outline()
                       ->DropdownCaret()
                       ->Label(StrL("Small Button"))
                       ->IntoEl());
    StorySectionAdd(drop, dropRow);
    page->Child(drop);

    El* hg = StorySection(a, "Horizontal group", nullptr);
    StorySectionAdd(hg, BtnGroup(a, app, false, "button-h"));
    page->Child(hg);

    El* vg = StorySection(a, "Vertical group", nullptr);
    StorySectionAdd(vg, BtnGroup(a, app, true, "button-v"));
    page->Child(vg);

    El* sel = StorySection(a, "Selection group",
                           "Groups support single or multiple selection.");
    El* selRow = Div(a)->FlexRow()->Border(1, th.border)->Radius(th.radius);
    selRow->Child(Btn(a, app, "disabled-toggle-button")
                      ->Label(StrL("Disabled"))
                      ->Compact()
                      ->Outline()
                      ->IntoEl());
    selRow->Child(Btn(a, app, "loading-toggle-button")
                      ->Label(StrL("Loading"))
                      ->Compact()
                      ->Outline()
                      ->IntoEl());
    selRow->Child(Btn(a, app, "selected-toggle-button")
                      ->Label(StrL("Selected"))
                      ->Compact()
                      ->Outline()
                      ->Selected(true)
                      ->IntoEl());
    selRow->Child(Btn(a, app, "compact-toggle-button")
                      ->Label(StrL("Compact"))
                      ->Compact()
                      ->Outline()
                      ->IntoEl());
    StorySectionAdd(sel, selRow);
    page->Child(sel);

    El* only = StorySection(a, "Icon-only",
                            "Compact actions can omit visible labels.");
    El* onlyRow = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    onlyRow->Child(Btn(a, app, "icon-button-primary")
                       ->Icon(IconName::Search)
                       ->Primary()
                       ->IntoEl());
    onlyRow->Child(Btn(a, app, "icon-button-secondary")
                       ->Icon(IconName::Info)
                       ->Loading(true)
                       ->IntoEl());
    onlyRow->Child(Btn(a, app, "icon-button-danger")
                       ->Icon(IconName::X)
                       ->Danger()
                       ->IntoEl());
    onlyRow->Child(Btn(a, app, "icon-button-small-primary")
                       ->Icon(IconName::Search)
                       ->Primary()
                       ->IntoEl());
    onlyRow->Child(Btn(a, app, "icon-button-outline")
                       ->Icon(IconName::Search)
                       ->Outline()
                       ->IntoEl());
    onlyRow->Child(Btn(a, app, "icon-button-ghost")
                       ->Icon(IconName::ArrowLeft)
                       ->Ghost()
                       ->IntoEl());
    StorySectionAdd(only, onlyRow);
    page->Child(only);

    El* csz = StorySection(
        a, "Custom size",
        "A fixed pixel size is available for compact icon actions.");
    StorySectionAdd(csz, component::Button::New(a, StrL("icon-button-9"))
                             ->Icon(IconName::Heart)
                             ->Ghost()
                             ->IntoEl()
                             ->W(24)
                             ->H(24));
    page->Child(csz);

    Rgba magenta = Rgb(0xd9, 0x46, 0xef);
    El* customSec = StorySection(a, "Custom color", nullptr);
    El* customRow = Div(a)->FlexRow()->Gap(8)->ItemsCenter()->Wrap();
    customRow->Child(Btn(a, app, "button-6-custom")
                         ->Custom(magenta)
                         ->Label(StrL("Custom Button"))
                         ->IntoEl());
    customRow->Child(Btn(a, app, "button-outline-6-custom")
                         ->Outline()
                         ->Custom(magenta)
                         ->Label(StrL("Outline Button"))
                         ->IntoEl());
    customRow->Child(Btn(a, app, "button-outline-6-custom-1")
                         ->Outline()
                         ->Icon(IconName::Bell)
                         ->Custom(magenta)
                         ->Label(StrL("Icon Button"))
                         ->IntoEl());
    StorySectionAdd(customSec, customRow);
    page->Child(customSec);
    return page;
}

void ButtonClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryButton, ButtonRender, ButtonClick);
