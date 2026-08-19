/* Unstyled calendar — crates/base/src/calendar.rs */

#include "gpui/gpui.h"

namespace gpui {

// Which grid a calendar is showing. Rust's CalendarView, and what its header
// switches between.
enum class CalendarView : uint8_t {
    Day,
    Month,
    Year
};

// Rust's CalendarState, minus the selection it also holds: the month a
// calendar is looking at, how many months it shows at once, and which page of
// the year grid is up.
struct CalendarState {
    int currentYear = 0;
    // 1..12, as Rust keeps it.
    int currentMonth = 1;
    int numberOfMonths = 1;
    // The year grid is paged, and `yearPageCount` is how many pages the year
    // range came to.
    int yearPage = 0;
    int yearPageCount = 0;
    CalendarView view = CalendarView::Day;
};

// prev_month / next_month. Stepping off either end of the year carries into
// the next one, which is the whole reason these are not `month += 1`.
void CalendarPrevMonth(CalendarState* s);
void CalendarNextMonth(CalendarState* s);

// The year grid's paging. Rust's movers answer whether they moved, so a header
// can leave its arrow disabled at the ends.
bool CalendarHasPrevYearPage(const CalendarState* s);
bool CalendarHasNextYearPage(const CalendarState* s);
bool CalendarPrevYearPage(CalendarState* s);
bool CalendarNextYearPage(CalendarState* s);

// days_in_month: where the grid for a month starts, as a day offset back from
// its first. Sunday-first, as Rust's default weekday is, and the grid then
// runs in whole weeks — which is what keeps a six-week month six weeks.
//
// `firstWeekday` is the weekday of the 1st, 0 for Sunday.
int CalendarGridOffset(int firstWeekday);
// How many cells the grid needs: whole weeks covering the month from that
// offset. Rust's div_ceil(7) * 7.
int CalendarGridCells(int offset, int daysInMonth);

struct Calendar {
    static El* New(Ctx* cx, Str id);
};
struct CalendarItem {
    static El* New(Ctx* cx, Str id = {}, Listener onClick = {});
};
} // namespace gpui
