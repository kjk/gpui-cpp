#include "Story.h"

// CalendarState::new starts on the current month with nothing selected; the
// calendar marks today on its own.
struct CalendarStory {
    int calYear = 0;
    int calMonth = 0;
    int calDay = 0;
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
    if (self->calMonth == 0) {
        LocalDate now = DateToday();
        self->calYear = now.year;
        self->calMonth = now.month;
    }
    El* page = Div(a)->FlexCol()->Gap(12)->W(kFill);
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
    El* months = Div(a)->FlexRow()->FlexWrap()->Gap(16);
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
