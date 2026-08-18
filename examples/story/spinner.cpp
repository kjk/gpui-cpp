#include "Story.h"

struct SpinnerStory {
    StoryToolbarState toolbar;

    static El* Render(SpinnerStory* self, Ctx* cx);
};

El* SpinnerStory::Render(SpinnerStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(cx, self));

    El* def =
        StorySection(cx, "Default", "An indeterminate loading indicator.");
    StorySectionAdd(def, component::Spinner::New(cx)
                             ->WithSize(self->toolbar.size)
                             ->IntoEl());
    page->Child(def);

    El* color = StorySection(cx, "Color",
                             "Use a color that suits the surrounding status.");
    El* colorRow = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    colorRow->Child(component::Spinner::New(cx)
                        ->WithSize(self->toolbar.size)
                        ->Color(th.blue)
                        ->IntoEl());
    colorRow->Child(component::Spinner::New(cx)
                        ->WithSize(self->toolbar.size)
                        ->Color(th.green)
                        ->IntoEl());
    StorySectionAdd(color, colorRow);
    page->Child(color);

    El* csz = StorySection(cx, "Custom size",
                           "A fixed pixel size is also supported.");
    StorySectionAdd(csz, component::Spinner::New(cx)->Size(64)->IntoEl());
    page->Child(csz);

    El* ic = StorySection(cx, "Icon", "Replace the default spinner glyph.");
    El* icRow = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    icRow->Child(component::Spinner::New(cx)
                     ->WithSize(self->toolbar.size)
                     ->Icon(IconName::Loader)
                     ->IntoEl());
    icRow->Child(component::Spinner::New(cx)
                     ->WithSize(self->toolbar.size)
                     ->Icon(IconName::Loader)
                     ->Color(th.cyan)
                     ->IntoEl());
    StorySectionAdd(ic, icRow);
    page->Child(ic);

    El* ease =
        StorySection(cx, "Easing", "Customize the rotation timing curve.");
    El* easeRow = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    easeRow->Child(component::Spinner::New(cx)
                       ->WithSize(self->toolbar.size)
                       ->Icon(IconName::Loader)
                       ->IntoEl());
    easeRow->Child(component::Spinner::New(cx)
                       ->WithSize(self->toolbar.size)
                       ->Icon(IconName::Loader)
                       ->Color(th.blue)
                       ->IntoEl());
    StorySectionAdd(ease, easeRow);
    page->Child(ease);
    return page;
}

STORY_PAGE(StorySpinner, SpinnerStory);
