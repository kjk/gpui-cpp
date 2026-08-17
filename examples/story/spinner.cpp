#include "Story.h"

El* SpinnerRender(StoryApp* app, Arena* a) {
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(a, app));

    El* def = StorySection(a, "Default", "An indeterminate loading indicator.");
    StorySectionAdd(def,
                    component::Spinner::New(a)->WithSize(app->size)->IntoEl());
    page->Child(def);

    El* color = StorySection(a, "Color",
                             "Use a color that suits the surrounding status.");
    El* colorRow = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    colorRow->Child(component::Spinner::New(a)
                        ->WithSize(app->size)
                        ->Color(th.blue)
                        ->IntoEl());
    colorRow->Child(component::Spinner::New(a)
                        ->WithSize(app->size)
                        ->Color(th.green)
                        ->IntoEl());
    StorySectionAdd(color, colorRow);
    page->Child(color);

    El* csz =
        StorySection(a, "Custom size", "A fixed pixel size is also supported.");
    StorySectionAdd(csz, component::Spinner::New(a)->Size(64)->IntoEl());
    page->Child(csz);

    El* ic = StorySection(a, "Icon", "Replace the default spinner glyph.");
    El* icRow = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    icRow->Child(component::Spinner::New(a)
                     ->WithSize(app->size)
                     ->Icon(IconName::Loader)
                     ->IntoEl());
    icRow->Child(component::Spinner::New(a)
                     ->WithSize(app->size)
                     ->Icon(IconName::Loader)
                     ->Color(Rgb(0x22, 0xd3, 0xee))
                     ->IntoEl());
    StorySectionAdd(ic, icRow);
    page->Child(ic);

    El* ease =
        StorySection(a, "Easing", "Customize the rotation timing curve.");
    El* easeRow = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    easeRow->Child(component::Spinner::New(a)
                       ->WithSize(app->size)
                       ->Icon(IconName::Loader)
                       ->IntoEl());
    easeRow->Child(component::Spinner::New(a)
                       ->WithSize(app->size)
                       ->Icon(IconName::Loader)
                       ->Color(th.blue)
                       ->IntoEl());
    StorySectionAdd(ease, easeRow);
    page->Child(ease);
    return page;
}

void SpinnerClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StorySpinner, SpinnerRender, SpinnerClick);
