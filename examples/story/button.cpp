#include "Story.h"

El* ButtonRender(StoryApp* app, Arena* a) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(a, app));

    El* vars = StorySection(a, "Variants",
                            "Visual treatments communicate action priority.");
    El* row = Div(a)->FlexRow()->Gap(8)->ItemsCenter()->Wrap();
    row->Child(component::Button::New(a, StrL("btn-default"))
                   ->Label(StrL("Default"))
                   ->WithSize(app->size)
                   ->IntoEl());
    row->Child(component::Button::New(a, StrL("btn-primary"))
                   ->Label(StrL("Primary"))
                   ->Primary()
                   ->WithSize(app->size)
                   ->IntoEl());
    row->Child(component::Button::New(a, StrL("btn-secondary"))
                   ->Label(StrL("Secondary"))
                   ->Secondary()
                   ->WithSize(app->size)
                   ->IntoEl());
    row->Child(component::Button::New(a, StrL("btn-danger"))
                   ->Label(StrL("Danger"))
                   ->Danger()
                   ->WithSize(app->size)
                   ->IntoEl());
    row->Child(component::Button::New(a, StrL("btn-warning"))
                   ->Label(StrL("Warning"))
                   ->Warning()
                   ->WithSize(app->size)
                   ->IntoEl());
    row->Child(component::Button::New(a, StrL("btn-success"))
                   ->Label(StrL("Success"))
                   ->Success()
                   ->WithSize(app->size)
                   ->IntoEl());
    row->Child(component::Button::New(a, StrL("btn-info"))
                   ->Label(StrL("Info"))
                   ->Info()
                   ->WithSize(app->size)
                   ->IntoEl());
    row->Child(component::Button::New(a, StrL("btn-ghost"))
                   ->Label(StrL("Ghost"))
                   ->Ghost()
                   ->WithSize(app->size)
                   ->IntoEl());
    row->Child(component::Button::New(a, StrL("btn-link"))
                   ->Label(StrL("Link"))
                   ->Link()
                   ->WithSize(app->size)
                   ->IntoEl());
    row->Child(component::Button::New(a, StrL("btn-text"))
                   ->Label(StrL("Text"))
                   ->Text()
                   ->WithSize(app->size)
                   ->IntoEl());
    StorySectionAdd(vars, row);
    page->Child(vars);

    El* out = StorySection(a, "Outline",
                           "Outlined treatments keep actions visually quiet.");
    El* row2 = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    row2->Child(component::Button::New(a, StrL("btn-out"))
                    ->Label(StrL("Outline"))
                    ->Outline()
                    ->WithSize(app->size)
                    ->IntoEl());
    row2->Child(component::Button::New(a, StrL("btn-out-d"))
                    ->Label(StrL("Disabled"))
                    ->Outline()
                    ->Disabled(true)
                    ->WithSize(app->size)
                    ->IntoEl());
    StorySectionAdd(out, row2);
    page->Child(out);

    El* ic = StorySection(a, "With icon", nullptr);
    StorySectionAdd(ic, component::Button::New(a, StrL("btn-icon"))
                            ->Label(StrL("Inbox"))
                            ->Icon(IconName::Inbox)
                            ->Primary()
                            ->WithSize(app->size)
                            ->IntoEl());
    page->Child(ic);
    return page;
}

void ButtonClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryButton, ButtonRender, ButtonClick);
