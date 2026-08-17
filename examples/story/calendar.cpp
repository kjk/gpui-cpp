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
    El* single = StorySection(a, "Single month", "Single-date selection.");
    StorySectionAdd(single, component::Calendar::New(a)
                                ->Year(app->calYear)
                                ->Month(app->calMonth)
                                ->Day(app->calDay)
                                ->OnPrev(MkFunc0(&CalPrev, app))
                                ->OnNext(MkFunc0(&CalNext, app))
                                ->OnDay(MkFunc1(&CalDay, app))
                                ->IntoEl());
    page->Child(single);

    El* multi =
        StorySection(a, "Multiple months", "Three months shown together.");
    El* months = Div(a)->FlexRow()->Gap(16)->Wrap();
    for (int i = 0; i < 3; i++) {
        int m = app->calMonth + i;
        int y = app->calYear;
        while (m > 12) {
            m -= 12;
            y++;
        }
        months->Child(component::Calendar::New(a)
                          ->Year(y)
                          ->Month(m)
                          ->Day(i == 0 ? app->calDay : 0)
                          ->OnPrev(MkFunc0(&CalPrev, app))
                          ->OnNext(MkFunc0(&CalNext, app))
                          ->OnDay(MkFunc1(&CalDay, app))
                          ->IntoEl());
    }
    StorySectionAdd(multi, months);
    page->Child(multi);

    El* dis =
        StorySection(a, "Disabled dates", "Recurring unavailable weekdays.");
    StorySectionAdd(dis, component::Calendar::New(a)
                             ->Year(app->calYear)
                             ->Month(app->calMonth)
                             ->Day(app->calDay)
                             ->OnPrev(MkFunc0(&CalPrev, app))
                             ->OnNext(MkFunc0(&CalNext, app))
                             ->OnDay(MkFunc1(&CalDay, app))
                             ->IntoEl());
    page->Child(dis);
    return page;
}

void CalendarClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryCalendar, CalendarRender, CalendarClick);
