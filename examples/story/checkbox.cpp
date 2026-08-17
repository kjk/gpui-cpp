#include "Story.h"

static void SetCheck0(StoryApp* app, bool v) {
    app->checks[0] = v;
}
static void SetCheck1(StoryApp* app, bool v) {
    app->checks[1] = v;
}
static void SetCheck2(StoryApp* app, bool v) {
    app->checks[2] = v;
}
static void SetCheck3(StoryApp* app, bool v) {
    app->checks[3] = v;
}
static void SetCheck4(StoryApp* app, bool v) {
    app->checks[4] = v;
}
static void SetCheck5(StoryApp* app, bool v) {
    app->checks[5] = v;
}

El* CheckboxRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(cx, app));

    El* def = StorySection(
        cx, "Default", "Checked and unchecked options can be mixed freely.");
    El* defCol = Div(a)->FlexCol()->Gap(8);
    defCol->Child(component::Checkbox::New(cx, StrL("updates"))
                      ->Label(StrL("Product updates"))
                      ->Checked(app->checks[0])
                      ->WithSize(app->size)
                      ->OnClick(MkFunc1(&SetCheck0, app))
                      ->IntoEl());
    defCol->Child(component::Checkbox::New(cx, StrL("remember"))
                      ->Label(StrL("Remember this device"))
                      ->Checked(app->checks[1])
                      ->WithSize(app->size)
                      ->OnClick(MkFunc1(&SetCheck1, app))
                      ->IntoEl());
    StorySectionAdd(def, defCol);
    page->Child(def);

    El* bare =
        StorySection(cx, "Without label",
                     "The label can be supplied by surrounding content.");
    StorySectionAdd(bare, component::Checkbox::New(cx, StrL("unlabelled"))
                              ->Checked(app->checks[2])
                              ->WithSize(app->size)
                              ->OnClick(MkFunc1(&SetCheck2, app))
                              ->IntoEl());
    page->Child(bare);

    El* dis = StorySection(cx, "Disabled",
                           "Both checked and unchecked values remain visible.");
    El* disRow = Div(a)->FlexRow()->Gap(24)->ItemsCenter();
    disRow->Child(component::Checkbox::New(cx, StrL("disabled-checked"))
                      ->Label(StrL("Checked"))
                      ->Checked(true)
                      ->Disabled(true)
                      ->WithSize(app->size)
                      ->IntoEl());
    disRow->Child(component::Checkbox::New(cx, StrL("disabled-unchecked"))
                      ->Label(StrL("Unchecked"))
                      ->Checked(false)
                      ->Disabled(true)
                      ->WithSize(app->size)
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
            ->Checked(app->checks[3])
            ->WithSize(app->size)
            ->W(320)
            ->OnClick(MkFunc1(&SetCheck3, app))
            ->IntoEl());
    labCol->Child(component::Checkbox::New(cx, StrL("wrapping"))
                      ->Label(StrL("Notify me when a new device signs in to "
                                   "my account"))
                      ->Checked(app->checks[5])
                      ->WithSize(app->size)
                      ->W(320)
                      ->OnClick(MkFunc1(&SetCheck5, app))
                      ->IntoEl());
    labCol
        ->Child(component::Checkbox::New(cx, StrL("markdown"))
                    ->Label(StrL("Accept the terms"))
                    ->Hint(StrL("Read the terms of service before continuing."))
                    ->Checked(app->checks[4])
                    ->WithSize(app->size)
                    ->W(320)
                    ->OnClick(MkFunc1(&SetCheck4, app))
                    ->IntoEl());
    StorySectionAdd(labs, labCol);
    page->Child(labs);
    return page;
}

void CheckboxClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryCheckbox, CheckboxRender, CheckboxClick);
