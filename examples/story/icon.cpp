#include "Story.h"

El* IconRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    (void)app;
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* icons = StorySection(
        cx, "Icons", "Common interface symbols from the bundled icon set.");
    IconName names[] = {IconName::Info,     IconName::Search,
                        IconName::Bot,      IconName::Settings,
                        IconName::Calendar, IconName::Folder,
                        IconName::Heart};
    El* row = Div(a)->FlexRow()->Gap(16)->ItemsCenter();
    for (int i = 0; i < 7; i++) {
        row->Child(IconEl(a, names[i], 20)->Fg(th.foreground));
    }
    StorySectionAdd(icons, row);
    page->Child(icons);

    El* color =
        StorySection(cx, "Color", "Icons inherit semantic foreground colors.");
    El* colorRow = Div(a)->FlexRow()->Gap(16)->ItemsCenter();
    colorRow->Child(IconEl(a, IconName::Maximize, 24)->Fg(th.green));
    colorRow->Child(IconEl(a, IconName::Minus, 24)->Fg(th.red));
    StorySectionAdd(color, colorRow);
    page->Child(color);

    El* btns = StorySection(cx, "Icon Buttons",
                            "Icons can be used as compact button content.");
    El* btnRow = Div(a)->FlexRow()->Gap(16)->ItemsCenter();
    btnRow->Child(component::Button::New(cx, StrL("like1"))
                      ->Icon(IconName::Heart)
                      ->Ghost()
                      ->IntoEl());
    btnRow->Child(component::Button::New(cx, StrL("like2"))
                      ->Icon(IconName::Heart)
                      ->Ghost()
                      ->IntoEl());
    btnRow->Child(component::Button::New(cx, StrL("like3"))
                      ->Icon(IconName::Heart)
                      ->Ghost()
                      ->IntoEl());
    StorySectionAdd(btns, btnRow);
    page->Child(btns);

    El* csz = StorySection(
        cx, "Custom Size",
        "Explicit dimensions support dense controls and counters.");
    StorySectionAdd(csz, component::Button::New(cx, StrL("button-with-size"))
                             ->Outline()
                             ->Label(StrL("10"))
                             ->Compact()
                             ->IntoEl()
                             ->W(20)
                             ->H(20));
    page->Child(csz);
    return page;
}

void IconClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryIcon, IconRender, IconClick);
