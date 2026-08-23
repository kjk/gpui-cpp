/* Themed calendar and date picker — crates/ui/src/time/ */

#include "ui/sizing.h"

namespace gpui {

namespace component {

struct Calendar {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    int year = 2026;
    int month = 1;
    int day = 1;
    int selectedYear = 0;
    int selectedMonth = 0;
    LocalDate rangeEnd = {};
    UiSize size = UiSize::Medium;
    int numberOfMonths = 1;
    CalendarView view = CalendarView::Day;
    int yearMin = 0;
    int yearMax = 0; // exclusive
    int yearPageStart = 0;
    DateMatcher disabledMatcher = {};
    // border_0().rounded_none().p_0(): what a DatePicker's popup asks for,
    // since the popup is the frame and a second one inside it would show.
    bool bare = false;
    Listener onDay;  // day of month
    Listener onDate; // DatePickerDateKey(LocalDate)
    Listener onPrev;
    Listener onNext;
    Listener onMonthToggle;
    Listener onYearToggle;
    Listener onMonth;
    Listener onYear;

    static Calendar* New(Ctx* cx);
    Calendar* Year(int y);
    Calendar* Month(int m);
    Calendar* Day(int d);
    Calendar* Selection(int y, int m, int d);
    Calendar* RangeEnd(int y, int m, int d);
    Calendar* WithSize(UiSize s);
    Calendar* NumberOfMonths(int count);
    Calendar* View(CalendarView value);
    Calendar* YearRange(int minYear, int maxYear, int pageStart);
    Calendar* DisabledMatcher(DateMatcher matcher);
    Calendar* Bare();
    Calendar* OnDay(Listener fn);
    Calendar* OnDate(Listener fn);
    Calendar* OnPrev(Listener fn);
    Calendar* OnNext(Listener fn);
    Calendar* OnMonthToggle(Listener fn);
    Calendar* OnYearToggle(Listener fn);
    Calendar* OnMonth(Listener fn);
    Calendar* OnYear(Listener fn);
    El* IntoEl();
};

struct DateRangePreset {
    Str label = {};
    LocalDate start = {};
    LocalDate end = {};
    intptr_t arg = 0;
};

// The two formats the story uses: %Y/%m/%d (the default) and %Y-%m-%d.
enum class DateFormat : uint8_t {
    Slash,
    Dash
};

struct DatePicker {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    int year = 2026;
    int month = 1;
    int day = 1; // 0: no date picked, so the placeholder shows
    int viewYear = 0;
    int viewMonth = 0;
    // The end of a range; year2 == 0 means a single date.
    int year2 = 0;
    int month2 = 0;
    int day2 = 0;
    Str placeholder = {};
    DateFormat format = DateFormat::Slash;
    UiSize size = UiSize::Medium;
    float width = kFill;
    // cleanable swaps the calendar icon for a clear button once a date is set.
    bool cleanable = false;
    bool appearance = true;
    bool focusRing = true;
    bool range = false;
    bool open = false;
    int numberOfMonths = 1;
    CalendarView calendarView = CalendarView::Day;
    int yearMin = 0;
    int yearMax = 0;
    int yearPageStart = 0;
    DateMatcher disabledMatcher = {};
    const DateRangePreset* presets = nullptr;
    int presetsCount = 0;
    Listener onToggle;
    Listener onDay;
    Listener onDate;
    Listener onClear;
    Listener onPrev;
    Listener onNext;
    Listener onMonthToggle;
    Listener onYearToggle;
    Listener onMonth;
    Listener onYear;
    Listener onPreset;

    static DatePicker* New(Ctx* cx);
    DatePicker* Id(Str value);
    DatePicker* Year(int y);
    DatePicker* Month(int m);
    DatePicker* Day(int d);
    DatePicker* View(int y, int m);
    DatePicker* RangeEnd(int y, int m, int d);
    DatePicker* Placeholder(Str s);
    DatePicker* Format(DateFormat f);
    DatePicker* WithSize(UiSize s);
    DatePicker* W(float v);
    DatePicker* Cleanable(bool v = true);
    DatePicker* Appearance(bool v);
    // FocusableExt::focus_ring: no focus appearance on this control.
    DatePicker* FocusRing(bool v);
    DatePicker* Range(bool v = true);
    DatePicker* NumberOfMonths(int count);
    DatePicker* CalendarMode(CalendarView value);
    DatePicker* YearRange(int minYear, int maxYear, int pageStart);
    DatePicker* DisabledMatcher(DateMatcher matcher);
    DatePicker* Presets(const DateRangePreset* values, int count,
                        Listener onSelect);
    DatePicker* Open(bool v);
    DatePicker* OnToggle(Listener fn);
    DatePicker* OnDay(Listener fn);
    DatePicker* OnDate(Listener fn);
    DatePicker* OnClear(Listener fn);
    DatePicker* OnPrev(Listener fn);
    DatePicker* OnNext(Listener fn);
    DatePicker* OnMonthToggle(Listener fn);
    DatePicker* OnYearToggle(Listener fn);
    DatePicker* OnMonth(Listener fn);
    DatePicker* OnYear(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
