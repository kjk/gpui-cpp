#include "Story.h"

struct IconStory {
    static El* Render(IconStory* self, Ctx* cx);
};

El* IconStory::Render(IconStory*, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* icons = StorySection(
        cx, "Icons", "Common interface symbols from the bundled icon set.");
    IconName names[] = {IconName::Info,   IconName::Map,      IconName::Bot,
                        IconName::Github, IconName::Calendar, IconName::Globe,
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
    colorRow->Child(IconEl(a, IconName::Minimize, 24)->Fg(th.red));
    StorySectionAdd(color, colorRow);
    page->Child(color);

    El* btns = StorySection(cx, "Icon Buttons",
                            "Icons can be used as compact button content.");
    El* btnRow = Div(a)->FlexRow()->Gap(16)->ItemsCenter();
    // A neutral heart, a red heart-off, a green heart — each a size_6 icon.
    btnRow->Child(component::Button::New(cx, StrL("like1"))
                      ->Ghost()
                      ->Extra(IconEl(a, IconName::Heart, 24)->Fg(th.mutedFg))
                      ->IntoEl());
    btnRow->Child(component::Button::New(cx, StrL("like2"))
                      ->Ghost()
                      ->Extra(IconEl(a, IconName::HeartOff, 24)->Fg(th.red))
                      ->IntoEl());
    btnRow->Child(component::Button::New(cx, StrL("like3"))
                      ->Ghost()
                      ->Extra(IconEl(a, IconName::Heart, 24)->Fg(th.green))
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

STORY_PAGE(StoryIcon, IconStory);
