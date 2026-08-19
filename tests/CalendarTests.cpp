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

void TestCalendar() {
    TestSuite("calendar");
    SteppingAMonthCarriesIntoTheYear();
    TheYearGridPagesAndStopsAtItsEnds();
    TheGridStartsOnTheWeekAndRunsInWholeWeeks();
}
