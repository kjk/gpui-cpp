#include "Story.h"

El* TagRender(StoryApp* app, Arena* a) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(a, app));
    El* sec = StorySection(a, "Variants", "Labels and categories.");
    El* row = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    row->Child(component::Tag::New(a, StrL("Primary"))
                   ->Primary()
                   ->WithSize(app->size)
                   ->IntoEl());
    row->Child(component::Tag::New(a, StrL("Secondary"))
                   ->Secondary()
                   ->WithSize(app->size)
                   ->IntoEl());
    row->Child(component::Tag::New(a, StrL("Success"))
                   ->Success()
                   ->WithSize(app->size)
                   ->IntoEl());
    row->Child(component::Tag::New(a, StrL("Warning"))
                   ->Warning()
                   ->WithSize(app->size)
                   ->IntoEl());
    row->Child(component::Tag::New(a, StrL("Danger"))
                   ->Danger()
                   ->WithSize(app->size)
                   ->IntoEl());
    row->Child(component::Tag::New(a, StrL("Info"))
                   ->Info()
                   ->WithSize(app->size)
                   ->IntoEl());
    StorySectionAdd(sec, row);
    page->Child(sec);

    El* out = StorySection(a, "Outline", nullptr);
    El* row2 = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    row2->Child(component::Tag::New(a, StrL("New"))
                    ->Success()
                    ->Outline()
                    ->WithSize(app->size)
                    ->IntoEl());
    row2->Child(component::Tag::New(a, StrL("Beta"))
                    ->Warning()
                    ->Outline()
                    ->WithSize(app->size)
                    ->IntoEl());
    StorySectionAdd(out, row2);
    page->Child(out);
    return page;
}

void TagClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryTag, TagRender, TagClick);
