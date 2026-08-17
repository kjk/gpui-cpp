#include "Story.h"

static El* BoxBody(Arena* a) {
    const Theme& th = ThemeNow();
    El* col = Div(a)->FlexCol()->Gap(8);
    col->Child(component::Checkbox::New(a, StrL("gb-dark"))
                   ->Label(StrL("Dark mode"))
                   ->Checked(true)
                   ->IntoEl());
    col->Child(component::Switch::New(a, StrL("gb-compact"))
                   ->Label(StrL("Compact density"))
                   ->Checked(false)
                   ->IntoEl());
    col->Child(StoryTxt(a, StrL("Theme, radius, and density live together."),
                        13, th.mutedFg));
    return col;
}

El* GroupBoxRender(StoryApp* app, Arena* a) {
    (void)app;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* def = StorySection(a, "Default", nullptr);
    StorySectionAdd(def, component::GroupBox::New(a, StrL("Appearance"))
                             ->Child(BoxBody(a))
                             ->IntoEl());
    page->Child(def);

    El* filled = StorySection(a, "Filled", nullptr);
    StorySectionAdd(filled, component::GroupBox::New(a, StrL("Appearance"))
                                ->Filled(true)
                                ->Child(BoxBody(a))
                                ->IntoEl());
    page->Child(filled);

    El* out = StorySection(a, "Outlined", nullptr);
    StorySectionAdd(out, component::GroupBox::New(a, StrL("Appearance"))
                             ->Outline()
                             ->Child(BoxBody(a))
                             ->IntoEl());
    page->Child(out);

    El* none = StorySection(a, "Without Title", nullptr);
    StorySectionAdd(none, component::GroupBox::New(a, {})
                              ->Outline()
                              ->Child(BoxBody(a))
                              ->IntoEl());
    page->Child(none);

    El* custom = StorySection(a, "Custom Style", nullptr);
    StorySectionAdd(custom, component::GroupBox::New(a, StrL("Appearance"))
                                ->Outline()
                                ->Child(BoxBody(a))
                                ->IntoEl());
    page->Child(custom);
    return page;
}

void GroupBoxClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryGroupBox, GroupBoxRender, GroupBoxClick);
