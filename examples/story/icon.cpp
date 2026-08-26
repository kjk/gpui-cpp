#include "Story.h"

struct IconStory {
    static El* Render(IconStory* self, Ctx* cx);
};

// Every section is .w(px(480.)).
static El* IconSection(Ctx* cx, const char* title, const char* desc) {
    El* sec = StorySection(cx, title, desc);
    StorySectionBody(sec)->W(480);
    return sec;
}

El* IconStory::Render(IconStory*, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow(cx->app);
    El* page = Div(a)->FlexCol()->ItemsCenter()->Gap(24)->W(kFill);

    // The icons are children of the section itself, which wraps them at
    // gap_4; .text_lg() is what sizes them, since an Icon is as big as the
    // text it inherits.
    El* icons = IconSection(
        cx, "Icons", "Common interface symbols from the bundled icon set.");
    StorySectionBody(icons)->Font(18);
    static const IconName kNames[] = {
        IconName::Info,     IconName::Map,   IconName::Bot,  IconName::Github,
        IconName::Calendar, IconName::Globe, IconName::Heart};
    for (IconName n : kNames) {
        StorySectionAdd(icons, IconEl(a, n, 18)->Fg(th.foreground));
    }
    page->Child(icons);

    El* color =
        IconSection(cx, "Color", "Icons inherit semantic foreground colors.");
    StorySectionAdd(color, IconEl(a, IconName::Maximize, 24)->Fg(th.green));
    StorySectionAdd(color, IconEl(a, IconName::Minimize, 24)->Fg(th.red));
    page->Child(color);

    El* btns = IconSection(cx, "Icon Buttons",
                           "Icons can be used as compact button content.");
    El* btnRow = Div(a)->FlexRow()->Gap(16)->ItemsCenter();
    // neutral_500, a red heart-off and a green heart. Each is the button's
    // own ButtonIcon rather than a child, which is what makes the button an
    // icon button — a 32px square. The `.size_6()` the Rust story writes on
    // each Icon does not survive: `.icon()` re-sizes it to the button's own.
    btnRow->Child(component::Button::New(cx, StrL("like1"))
                      ->Ghost()
                      ->Icon(IconName::Heart)
                      ->IconColor(Rgb(0x73, 0x73, 0x73))
                      ->IntoEl());
    btnRow->Child(component::Button::New(cx, StrL("like2"))
                      ->Ghost()
                      ->Icon(IconName::HeartOff)
                      ->IconColor(th.red)
                      ->IntoEl());
    btnRow->Child(component::Button::New(cx, StrL("like3"))
                      ->Ghost()
                      ->Icon(IconName::Heart)
                      ->IconColor(th.green)
                      ->IntoEl());
    StorySectionAdd(btns, btnRow);
    page->Child(btns);

    // Button::size_5().small().px_0(): a 20px square with a label in it.
    El* csz =
        IconSection(cx, "Custom Size",
                    "Explicit dimensions support dense controls and counters.");
    StorySectionAdd(csz, component::Button::New(cx, StrL("button-with-size"))
                             ->Outline()
                             ->Size(20)
                             ->WithSize(UiSize::Small)
                             ->Label(StrL("10"))
                             ->IntoEl());
    page->Child(csz);
    return page;
}

STORY_PAGE(StoryIcon, IconStory);
