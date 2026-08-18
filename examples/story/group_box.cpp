#include "Story.h"

struct GroupBoxStory {
    static El* Render(GroupBoxStory* self, Ctx* cx);
    static void Click(GroupBoxStory* self, Ctx* cx, int id);
};

static El* BoxBody(Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow();
    El* col = Div(a)->FlexCol()->Gap(8);
    col->Child(component::Checkbox::New(cx, StrL("gb-dark"))
                   ->Label(StrL("Dark mode"))
                   ->Checked(true)
                   ->IntoEl());
    col->Child(component::Switch::New(cx, StrL("gb-compact"))
                   ->Label(StrL("Compact density"))
                   ->Checked(false)
                   ->IntoEl());
    col->Child(StoryTxt(cx, StrL("Theme, radius, and density live together."),
                        13, th.mutedFg));
    return col;
}

El* GroupBoxStory::Render(GroupBoxStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* def = StorySection(cx, "Default", nullptr);
    StorySectionAdd(def, component::GroupBox::New(cx, StrL("Appearance"))
                             ->Child(BoxBody(cx))
                             ->IntoEl());
    page->Child(def);

    El* filled = StorySection(cx, "Filled", nullptr);
    StorySectionAdd(filled, component::GroupBox::New(cx, StrL("Appearance"))
                                ->Filled(true)
                                ->Child(BoxBody(cx))
                                ->IntoEl());
    page->Child(filled);

    El* out = StorySection(cx, "Outlined", nullptr);
    StorySectionAdd(out, component::GroupBox::New(cx, StrL("Appearance"))
                             ->Outline()
                             ->Child(BoxBody(cx))
                             ->IntoEl());
    page->Child(out);

    El* none = StorySection(cx, "Without Title", nullptr);
    StorySectionAdd(none, component::GroupBox::New(cx, {})
                              ->Outline()
                              ->Child(BoxBody(cx))
                              ->IntoEl());
    page->Child(none);

    El* custom = StorySection(cx, "Custom Style", nullptr);
    StorySectionAdd(custom, component::GroupBox::New(cx, StrL("Appearance"))
                                ->Outline()
                                ->Child(BoxBody(cx))
                                ->IntoEl());
    page->Child(custom);
    return page;
}

void GroupBoxStory::Click(GroupBoxStory* self, Ctx* cx, int id) {
    (void)cx;
    (void)id;
}

STORY_PAGE(StoryGroupBox, GroupBoxStory);
