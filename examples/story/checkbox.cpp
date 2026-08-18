#include "Story.h"

struct CheckboxStory {
    bool checks[8] = {false, true};
    StoryToolbarState toolbar;

    static El* Render(CheckboxStory* self, Ctx* cx);
};

static void SetCheck0(CheckboxStory* self, Ctx*, const ClickEvent*,
                      intptr_t v) {
    self->checks[0] = v;
}
static void SetCheck1(CheckboxStory* self, Ctx*, const ClickEvent*,
                      intptr_t v) {
    self->checks[1] = v;
}
static void SetCheck2(CheckboxStory* self, Ctx*, const ClickEvent*,
                      intptr_t v) {
    self->checks[2] = v;
}
static void SetCheck3(CheckboxStory* self, Ctx*, const ClickEvent*,
                      intptr_t v) {
    self->checks[3] = v;
}
static void SetCheck4(CheckboxStory* self, Ctx*, const ClickEvent*,
                      intptr_t v) {
    self->checks[4] = v;
}
static void SetCheck5(CheckboxStory* self, Ctx*, const ClickEvent*,
                      intptr_t v) {
    self->checks[5] = v;
}

El* CheckboxStory::Render(CheckboxStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(12)->W(kFill);
    page->Child(StoryToolbar(cx, self));

    El* def = StorySection(
        cx, "Default", "Checked and unchecked options can be mixed freely.");
    // Each checkbox is its own child of the section, which lays them out in
    // a wrapping row.
    StorySectionAdd(def, component::Checkbox::New(cx, StrL("updates"))
                             ->Label(StrL("Product updates"))
                             ->Checked(self->checks[0])
                             ->WithSize(self->toolbar.size)
                             ->OnClick(Listen(cx, &SetCheck0))
                             ->IntoEl());
    StorySectionAdd(def, component::Checkbox::New(cx, StrL("remember"))
                             ->Label(StrL("Remember this device"))
                             ->Checked(self->checks[1])
                             ->WithSize(self->toolbar.size)
                             ->OnClick(Listen(cx, &SetCheck1))
                             ->IntoEl());
    page->Child(def);

    El* bare =
        StorySection(cx, "Without label",
                     "The label can be supplied by surrounding content.");
    StorySectionAdd(bare, component::Checkbox::New(cx, StrL("unlabelled"))
                              ->Checked(self->checks[2])
                              ->WithSize(self->toolbar.size)
                              ->OnClick(Listen(cx, &SetCheck2))
                              ->IntoEl());
    page->Child(bare);

    El* dis = StorySection(cx, "Disabled",
                           "Both checked and unchecked values remain visible.");
    El* disRow = Div(a)->FlexRow()->Gap(24)->ItemsCenter();
    disRow->Child(component::Checkbox::New(cx, StrL("disabled-checked"))
                      ->Label(StrL("Checked"))
                      ->Checked(true)
                      ->Disabled(true)
                      ->WithSize(self->toolbar.size)
                      ->IntoEl());
    disRow->Child(component::Checkbox::New(cx, StrL("disabled-unchecked"))
                      ->Label(StrL("Unchecked"))
                      ->Checked(false)
                      ->Disabled(true)
                      ->WithSize(self->toolbar.size)
                      ->IntoEl());
    StorySectionAdd(dis, disRow);
    page->Child(dis);

    El* labs = StorySection(cx, "Labels",
                            "Labels can wrap and include supporting content.");
    El* labCol = Div(a)->FlexCol()->Gap(20)->W(320);
    labCol->Child(
        component::Checkbox::New(cx, StrL("description"))
            ->Label(StrL("Automatic updates"))
            ->Hint(StrL("Download updates when the application is idle."))
            ->Checked(self->checks[3])
            ->WithSize(self->toolbar.size)
            ->W(320)
            ->OnClick(Listen(cx, &SetCheck3))
            ->IntoEl());
    labCol->Child(component::Checkbox::New(cx, StrL("wrapping"))
                      ->Label(StrL("Notify me when a new device signs in to "
                                   "my account"))
                      ->Checked(self->checks[5])
                      ->WithSize(self->toolbar.size)
                      ->W(320)
                      ->OnClick(Listen(cx, &SetCheck5))
                      ->IntoEl());
    labCol
        ->Child(component::Checkbox::New(cx, StrL("markdown"))
                    ->Label(StrL("Accept the terms"))
                    ->Hint(StrL("Read the terms of service before continuing."))
                    ->Checked(self->checks[4])
                    ->WithSize(self->toolbar.size)
                    ->W(320)
                    ->OnClick(Listen(cx, &SetCheck4))
                    ->IntoEl());
    StorySectionAdd(labs, labCol);
    page->Child(labs);
    return page;
}

STORY_PAGE(StoryCheckbox, CheckboxStory);
