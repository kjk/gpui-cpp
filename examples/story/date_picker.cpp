#include "Story.h"

static void ToggleDate(StoryApp* app) {
    app->dateOpen = !app->dateOpen;
}
static void PickDate(StoryApp* app, int d) {
    app->calDay = d;
    app->dateOpen = false;
}

El* DatePickerRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(cx, "Default",
                           "A date picker component with range and presets.");
    StorySectionAdd(sec, component::DatePicker::New(cx)
                             ->Year(app->calYear)
                             ->Month(app->calMonth)
                             ->Day(app->calDay)
                             ->Open(app->dateOpen)
                             ->OnToggle(MkFunc0(&ToggleDate, app))
                             ->OnDay(MkFunc1(&PickDate, app))
                             ->IntoEl());
    page->Child(sec);

    El* dis = StorySection(cx, "Disabled dates", nullptr);
    StorySectionAdd(dis, component::DatePicker::New(cx)
                             ->Year(app->calYear)
                             ->Month(app->calMonth)
                             ->Day(app->calDay)
                             ->IntoEl());
    page->Child(dis);

    El* range = StorySection(cx, "Date range", nullptr);
    StorySectionAdd(range, component::DatePicker::New(cx)
                               ->Year(app->calYear)
                               ->Month(app->calMonth)
                               ->Day(app->calDay)
                               ->IntoEl());
    page->Child(range);

    El* empty = StorySection(cx, "Empty range", nullptr);
    StorySectionAdd(empty, component::DatePicker::New(cx)
                               ->Year(app->calYear)
                               ->Month(app->calMonth)
                               ->Day(0)
                               ->IntoEl());
    page->Child(empty);

    El* year = StorySection(cx, "Year range", nullptr);
    StorySectionAdd(year, component::DatePicker::New(cx)
                              ->Year(app->calYear)
                              ->Month(app->calMonth)
                              ->Day(app->calDay)
                              ->IntoEl());
    page->Child(year);

    El* style = StorySection(cx, "Custom style", nullptr);
    StorySectionAdd(style, component::DatePicker::New(cx)
                               ->Year(app->calYear)
                               ->Month(app->calMonth)
                               ->Day(app->calDay)
                               ->IntoEl());
    page->Child(style);
    return page;
}

void DatePickerClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryDatePicker, DatePickerRender, DatePickerClick);
