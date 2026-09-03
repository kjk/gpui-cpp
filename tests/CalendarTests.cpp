/* Ported from crates/base/src/calendar.rs.
 *
 * The month steps and the year grid's paging are the state's, and its own
 * tests there drive them through a window. The grid arithmetic is
 * days_in_month, which is what keeps a six-week month six weeks. */

#include "Test.h"

static void SteppingAMonthCarriesIntoTheYear() {
    CalendarState s;
    s.currentYear = 2026;
    s.currentMonth = 1;
    CalendarPrevMonth(&s);
    utassert(s.currentYear == 2025 && s.currentMonth == 12);
    CalendarNextMonth(&s);
    utassert(s.currentYear == 2026 && s.currentMonth == 1);

    s.currentMonth = 12;
    CalendarNextMonth(&s);
    utassert(s.currentYear == 2027 && s.currentMonth == 1);

    // An ordinary step touches the year not at all.
    s.currentYear = 2026;
    s.currentMonth = 6;
    CalendarNextMonth(&s);
    utassert(s.currentYear == 2026 && s.currentMonth == 7);
    CalendarPrevMonth(&s);
    utassert(s.currentYear == 2026 && s.currentMonth == 6);
}

static void TheYearGridPagesAndStopsAtItsEnds() {
    CalendarState s;
    s.yearPageCount = 3;
    utassert(!CalendarHasPrevYearPage(&s));
    utassert(CalendarHasNextYearPage(&s));
    // The movers answer whether they moved, so a header can leave an arrow
    // disabled at the ends.
    utassert(!CalendarPrevYearPage(&s));
    utassert(CalendarNextYearPage(&s));
    utassert(s.yearPage == 1);
    utassert(CalendarNextYearPage(&s));
    utassert(s.yearPage == 2);
    utassert(!CalendarNextYearPage(&s));
    utassert(s.yearPage == 2);
    utassert(CalendarPrevYearPage(&s));
    utassert(s.yearPage == 1);
}

static void TheGridStartsOnTheWeekAndRunsInWholeWeeks() {
    // A month starting on Sunday needs no lead-in.
    utassert(CalendarGridOffset(0) == 0);
    // One starting on Wednesday backs up three days to the Sunday.
    utassert(CalendarGridOffset(3) == 3);
    utassert(CalendarGridOffset(6) == 6);

    // February of a common year starting on a Sunday is exactly four weeks.
    utassert(CalendarGridCells(0, 28) == 28);
    // A 31-day month starting on Saturday spills into a sixth week.
    utassert(CalendarGridCells(6, 31) == 42);
    // And a 30-day month starting on Sunday needs five.
    utassert(CalendarGridCells(0, 30) == 35);
}

static LocalDate CalDate(int year, int month, int day) {
    return {year, month, day};
}

static bool BlocksFourth(LocalDate date) {
    return date.day == 4;
}

static void DateAndMatcherRetainTheirPayloadSemantics() {
    Date empty = Date::Single();
    Date single = Date::Single(CalDate(2025, 4, 4));
    Date partial = Date::Range(CalDate(2025, 4, 4));
    Date range = Date::Range(CalDate(2025, 4, 4), CalDate(2025, 4, 8));
    utassert(!empty.IsSome() && !empty.IsComplete());
    utassert(single.IsSome() && single.IsComplete() && single.IsSingle());
    utassert(partial.IsSome() && !partial.IsComplete());
    utassert(range.IsComplete() && !range.IsSingle());
    utassert(range.IsActive(CalDate(2025, 4, 4)));
    utassert(range.IsActive(CalDate(2025, 4, 8)));
    utassert(range.IsInRange(CalDate(2025, 4, 6)));
    utassert(!range.IsInRange(CalDate(2025, 4, 9)));

    Matcher custom = DateMatcherCustom(&BlocksFourth);
    // Matcher::is_match ignores incomplete ranges, but checks either endpoint
    // of a complete one.
    utassert(!MatcherMatches(custom, partial));
    utassert(MatcherMatches(custom, range));
    utassert(DateMatcherMatches(custom, CalDate(2025, 5, 4)));
}

namespace {
struct CalendarSink {
    int events = 0;
    Date last = {};

    static void OnSelected(CalendarSink* self, Ctx*, const CalendarEvent* ev) {
        self->events++;
        self->last = ev->date;
    }
};

struct WeekdayLabels {
    int values[7] = {};
    int count = 0;
};
} // namespace

static Str CaptureWeekday(void* user, Ctx*, CalendarItemKind kind, int value) {
    WeekdayLabels* labels = (WeekdayLabels*)user;
    if (kind == CalendarItemKind::Weekday && labels->count < 7) {
        labels->values[labels->count++] = value;
    }
    return {};
}

static void RetainedStateOwnsSelectionEventsAndRendering() {
    App app = {};
    Window* win = new Window();
    win->app = &app;
    Arena* a = ArenaNew();
    Ctx cx = {&app, win, a, {}};

    Entity<CalendarState> calendar = CalendarStateNew(&cx, Date::Range());
    Entity<CalendarSink> sink = EntityNewState<CalendarSink>(&app);
    SubscribeTo(&app, calendar, sink, &CalendarSink::OnSelected);
    CalendarState* state = calendar.Get(&app);
    utassert(state && state->focus.id != 0);
    utassert(state && state->self.id == calendar.id);

    bool first = CalendarStateSelectDate(state, CalDate(2025, 4, 4), &cx);
    utassert(!first && sink.Get(&app)->events == 0);
    bool second = CalendarStateSelectDate(state, CalDate(2025, 4, 8), &cx);
    utassert(second && sink.Get(&app)->events == 1);
    utassert(sink.Get(&app)->last.IsComplete());

    CalendarStateSetDisabledMatcher(state, DateMatcherCustom(&BlocksFourth));
    CalendarStateSetDate(state, Date::Single(CalDate(2025, 5, 4)), &cx);
    utassert(state->date.kind == DateKind::Range);

    CalendarStateSetDate(state, Date::Single(CalDate(2024, 8, 1)), &cx);
    WeekdayLabels labels;
    Style refined = {};
    refined.height = 333;
    El* root = Calendar::New(&cx, StrL("state-calendar"), calendar)
                   ->NumberOfMonths(2)
                   ->FirstDayOfWeek(1)
                   ->Label(&CaptureWeekday, &labels)
                   ->Refine(refined, StyleFieldHeight)
                   ->IntoEl();
    utassert(root && root->style.focusId == state->focus.id);
    utassert(root && root->style.height == 333);
    utassert(state->numberOfMonths == 2);
    utassert(labels.count == 7);
    // The callback records at most seven: Monday first, Sunday last.
    utassert(labels.values[0] == 1 && labels.values[6] == 0);

    ArenaDelete(a);
    delete win;
    EntityDropAll(&app);
}

void TestCalendar() {
    TestSuite("calendar");
    SteppingAMonthCarriesIntoTheYear();
    TheYearGridPagesAndStopsAtItsEnds();
    TheGridStartsOnTheWeekAndRunsInWholeWeeks();
    DateAndMatcherRetainTheirPayloadSemantics();
    RetainedStateOwnsSelectionEventsAndRendering();
}
