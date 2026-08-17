#include "Story.h"

static void CalPrev(StoryApp* app) {
    app->calMonth--;
    if (app->calMonth < 1) {
        app->calMonth = 12;
        app->calYear--;
    }
}
static void CalNext(StoryApp* app) {
    app->calMonth++;
    if (app->calMonth > 12) {
        app->calMonth = 1;
        app->calYear++;
    }
}
static void CalDay(StoryApp* app, int d) {
    app->calDay = d;
}

El* CalendarRender(StoryApp* app, Arena* a) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default", "A calendar of days displayed in a grid.");
    StorySectionAdd(sec, component::Calendar::New(a)
                             ->Year(app->calYear)
                             ->Month(app->calMonth)
                             ->Day(app->calDay)
                             ->OnPrev(MkFunc0(&CalPrev, app))
                             ->OnNext(MkFunc0(&CalNext, app))
                             ->OnDay(MkFunc1(&CalDay, app))
                             ->IntoEl());
    page->Child(sec);
    return page;
}

void CalendarClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryCalendar, CalendarRender, CalendarClick);
