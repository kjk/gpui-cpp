/* Unstyled calendar — crates/base/src/calendar.rs */

#include "gpui/gpui.h"
#include "base/date_picker.h"

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

// CalendarItemKind: what a slot in the calendar stands for. Every element a
// calendar builds is one of these, and every one of them is handed to the
// caller to decorate.
enum class CalendarItemKind : uint8_t {
    Previous,
    MonthToggle,
    YearToggle,
    Next,
    Weekday,
    Day,
    Month,
    Year
};

// CalendarItemState: the slot, and everything about it a look could depend
// on. Rust keeps the fields private behind readers so a new one can be added
// without breaking the item slots; the same care applies here, which is why
// a caller is handed this rather than the six flags.
struct CalendarItemState {
    CalendarItemKind kind = CalendarItemKind::Day;
    // The day of the month, the month, the year, or the weekday's index. The
    // two arrows have none.
    int value = 0;
    // Day only: the date the cell stands for, which may be in a neighbouring
    // month.
    LocalDate date = {};
    bool active = false;
    bool inRange = false;
    bool muted = false;
    bool disabled = false;
    bool today = false;
};

// `Calendar::item(|item, state, window, cx| ...)`. The calendar builds the
// slot — its size, its place in the grid, its id and its click — and hands it
// over to be decorated; what comes back is what goes in the grid, which is
// normally the same element with a look and a label on it. A function and a
// user pointer, since an element here holds no closures.
using CalendarItemFn = El* (*)(void* user, Ctx* cx, El* item,
                               const CalendarItemState& st);

// What the calendar is looking at and what it may do — everything except how
// it looks, which is the item function's.
struct CalendarOpts {
    int year = 0;
    int month = 1;
    int numberOfMonths = 1;
    CalendarView view = CalendarView::Day;
    // The square a day, a weekday head and an arrow are drawn in.
    float cellSize = 32;
    // The selection, and the far end of it for a range. A `day` of 0 is no
    // selection at all.
    LocalDate selected = {};
    LocalDate rangeEnd = {};
    // Today, for the cell that is outlined. The caller passes it rather than
    // the calendar asking, so a test can say what day it is.
    LocalDate today = {};
    DateMatcher disabledMatcher = {};
    // The year grid's range and which page of it is up.
    int yearMin = 0;
    int yearMax = 0; // exclusive
    int yearPageStart = 0;
    Listener onDay = {};  // day of month
    Listener onDate = {}; // DatePickerDateKey(LocalDate)
    Listener onPrev = {};
    Listener onNext = {};
    Listener onMonthToggle = {};
    Listener onYearToggle = {};
    Listener onMonth = {};
    Listener onYear = {};
    CalendarItemFn item = nullptr;
    void* user = nullptr;
};

struct Calendar {
    // The bare box, for a caller that builds the grid itself.
    static El* New(Ctx* cx, Str id);
    // The calendar: the header with its two arrows and its month and year
    // toggles, and below it the day grid, the month picker or the year
    // picker. The root carries no padding or border of its own — the caller
    // chains those onto what comes back.
    static El* New(Ctx* cx, Str id, const CalendarOpts& o);
};

// The month a calendar `offset` months on from the one it is looking at.
// Stepping off either end of the year carries, which is the whole reason this
// is not `month + offset`.
void CalendarOffsetMonth(int year, int month, int offset, int* outYear,
                         int* outMonth);
// The weekday of a date, 0 for Sunday — Sakamoto, which is what puts the 1st
// under the right head.
int CalendarWeekday(int year, int month, int day);
// How many days a month has, leap years counted.
int CalendarDaysInMonth(int year, int month);

struct CalendarItem {
    static El* New(Ctx* cx, Str id = {}, Listener onClick = {});
};
} // namespace gpui
