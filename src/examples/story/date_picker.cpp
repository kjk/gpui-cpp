#include "Story.h"

static void ToggleDate(StoryApp* app) {
    app->dateOpen = !app->dateOpen;
}
static void PickDate(StoryApp* app, int d) {
    app->calDay = d;
    app->dateOpen = false;
}

El* DatePickerRender(StoryApp* app, Arena* a) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default", "A date picker component with range and presets.");
    StorySectionAdd(sec, component::DatePicker::New(a)
                             ->Year(app->calYear)
                             ->Month(app->calMonth)
                             ->Day(app->calDay)
                             ->Open(app->dateOpen)
                             ->OnToggle(MkFunc0(&ToggleDate, app))
                             ->OnDay(MkFunc1(&PickDate, app))
                             ->IntoEl());
    page->Child(sec);
    return page;
}

void DatePickerClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryDatePicker, DatePickerRender, DatePickerClick);
