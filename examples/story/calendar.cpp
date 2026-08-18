#include "Story.h"

struct CalendarStory {
    int calYear = 2026;
    int calMonth = 8;
    int calDay = 17;
    static El* Render(CalendarStory* self, Ctx* cx);
};

static void CalPrev(CalendarStory* self, Ctx* cx, const ClickEvent*) {
    self->calMonth--;
    if (self->calMonth < 1) {
        self->calMonth = 12;
        self->calYear--;
    }
}
static void CalNext(CalendarStory* self, Ctx* cx, const ClickEvent*) {
    self->calMonth++;
    if (self->calMonth > 12) {
        self->calMonth = 1;
        self->calYear++;
    }
}
static void CalDay(CalendarStory* self, Ctx* cx, const ClickEvent*,
                   intptr_t d) {
    self->calDay = d;
}

El* CalendarStory::Render(CalendarStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* single = StorySection(cx, "Single month", "Single-date selection.");
    StorySectionAdd(single, component::Calendar::New(cx)
                                ->Year(self->calYear)
                                ->Month(self->calMonth)
                                ->Day(self->calDay)
                                ->OnPrev(Listen(cx, &CalPrev))
                                ->OnNext(Listen(cx, &CalNext))
                                ->OnDay(Listen(cx, &CalDay))
                                ->IntoEl());
    page->Child(single);

    El* multi =
        StorySection(cx, "Multiple months", "Three months shown together.");
    El* months = Div(a)->FlexRow()->Gap(16)->Wrap();
    for (int i = 0; i < 3; i++) {
        int m = self->calMonth + i;
        int y = self->calYear;
        while (m > 12) {
            m -= 12;
            y++;
        }
        months->Child(component::Calendar::New(cx)
                          ->Year(y)
                          ->Month(m)
                          ->Day(i == 0 ? self->calDay : 0)
                          ->OnPrev(Listen(cx, &CalPrev))
                          ->OnNext(Listen(cx, &CalNext))
                          ->OnDay(Listen(cx, &CalDay))
                          ->IntoEl());
    }
    StorySectionAdd(multi, months);
    page->Child(multi);

    El* dis =
        StorySection(cx, "Disabled dates", "Recurring unavailable weekdays.");
    StorySectionAdd(dis, component::Calendar::New(cx)
                             ->Year(self->calYear)
                             ->Month(self->calMonth)
                             ->Day(self->calDay)
                             ->OnPrev(Listen(cx, &CalPrev))
                             ->OnNext(Listen(cx, &CalNext))
                             ->OnDay(Listen(cx, &CalDay))
                             ->IntoEl());
    page->Child(dis);
    return page;
}

STORY_PAGE(StoryCalendar, CalendarStory);
