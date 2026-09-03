#ifndef GPUI_SRC_UI_TIME_H_
#define GPUI_SRC_UI_TIME_H_
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
    Entity<CalendarState> state = {};
    int firstDayOfWeek = 0;
    Style style = {};
    uint32_t styleSet = 0;
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
    // The source-shaped facade: behavior stays in Base CalendarState and
    // this layer supplies only labels, sizes and theme tokens.
    static Calendar* New(Ctx* cx, Entity<CalendarState> state);
    Calendar* Year(int y);
    Calendar* Month(int m);
    Calendar* Day(int d);
    Calendar* Selection(int y, int m, int d);
    Calendar* RangeEnd(int y, int m, int d);
    Calendar* WithSize(UiSize s);
    Calendar* NumberOfMonths(int count);
    Calendar* FirstDayOfWeek(int weekday);
    Calendar* View(CalendarView value);
    Calendar* YearRange(int minYear, int maxYear, int pageStart);
    Calendar* DisabledMatcher(DateMatcher matcher);
    Calendar* Bare();
    Calendar* Refine(const Style& value, uint32_t fields);
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

enum class DatePickerEventKind : uint8_t {
    Change
};

// DatePickerEvent::Change(Date).
struct DatePickerEvent {
    DatePickerEventKind kind = DatePickerEventKind::Change;
    Date date = {};
};

enum class DateRangePresetValueKind : uint8_t {
    Single,
    Range
};

struct DateRangePresetValue {
    DateRangePresetValueKind kind = DateRangePresetValueKind::Single;
    LocalDate start = {};
    LocalDate end = {};

    static DateRangePresetValue Single(LocalDate date);
    static DateRangePresetValue Range(LocalDate start, LocalDate end);
    Date IntoDate() const;
};

struct DateRangePreset {
    Str label = {};
    DateRangePresetValue value = {};
    // Compatibility fields from the earlier controlled builder. New code
    // uses `value`; old aggregate initialization continues to render.
    LocalDate start = {};
    LocalDate end = {};
    intptr_t arg = 0;

    static DateRangePreset Single(Str label, LocalDate date, intptr_t arg = 0);
    static DateRangePreset Range(Str label, LocalDate start, LocalDate end,
                                 intptr_t arg = 0);
};

// The two formats the story uses: %Y/%m/%d (the default) and %Y-%m-%d.
enum class DateFormat : uint8_t {
    Slash,
    Dash
};

// The retained state in crates/ui/src/time/date_picker.rs. The picker owns
// its Base calendar entity and forwards its completed selections as
// DatePickerEvent::Change.
struct DatePickerState {
    Entity<DatePickerState> self = {};
    FocusHandle focus = {};
    Date date = {};
    bool open = false;
    Entity<CalendarState> calendar = {};
    // Heap-owned because this state outlives every frame arena.
    Str dateFormat = {};
    int numberOfMonths = 1;
    Matcher disabledMatcher = {};
    Subscription calendarSubscription = {};
    int firstDayOfWeek = 0;
    Bounds bounds = {};

    ~DatePickerState();

    static void OnCalendar(DatePickerState* self, Ctx* cx,
                           const CalendarEvent* ev);
    static void OnToggle(DatePickerState* self, Ctx* cx, const ClickEvent* ev);
    static void OnOpenChange(DatePickerState* self, Ctx* cx,
                             const ClickEvent* ev, intptr_t open);
    static void OnDismiss(DatePickerState* self, Ctx* cx,
                          const MouseUpEvent* ev);
    static void OnClear(DatePickerState* self, Ctx* cx, const ClickEvent* ev);
};

Entity<DatePickerState> DatePickerStateNew(Ctx* cx, bool range = false);
inline Entity<DatePickerState> DatePickerStateRange(Ctx* cx) {
    return DatePickerStateNew(cx, true);
}
void DatePickerStateSetDate(DatePickerState* state, Date date, Ctx* cx,
                            bool emit = false);
void DatePickerStateSetDateFormat(DatePickerState* state, Str format,
                                  Ctx* cx = nullptr);
void DatePickerStateSetNumberOfMonths(DatePickerState* state, int count,
                                      Ctx* cx = nullptr);
void DatePickerStateSetFirstDayOfWeek(DatePickerState* state, int weekday,
                                      Ctx* cx = nullptr);
void DatePickerStateSetDisabledMatcher(DatePickerState* state, Matcher matcher,
                                       Ctx* cx = nullptr);
void DatePickerStateSetYearRange(DatePickerState* state, int minYear,
                                 int maxYear, Ctx* cx = nullptr);
void DatePickerStateSelectPreset(DatePickerState* state,
                                 const DateRangePreset& preset, Ctx* cx,
                                 bool emit = true);

// chrono's formatting seam, kept dependency-free. It covers the numeric,
// name and weekday directives used by gpui-component and copies unknown
// directives literally instead of silently changing the requested pattern.
Str DatePickerFormatDate(Arena* a, Str pattern, LocalDate date);
Str DatePickerFormatValue(Arena* a, Str pattern, Date date);

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
    bool disabled = false;
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
    Entity<DatePickerState> state = {};
    Style style = {};
    uint32_t styleSet = 0;

    static DatePicker* New(Ctx* cx);
    static DatePicker* New(Ctx* cx, Entity<DatePickerState> state);
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
    DatePicker* Refine(const Style& value, uint32_t fields);
    DatePicker* Disabled(bool v = true);
    DatePicker* Range(bool v = true);
    DatePicker* NumberOfMonths(int count);
    DatePicker* CalendarMode(CalendarView value);
    DatePicker* YearRange(int minYear, int maxYear, int pageStart);
    DatePicker* DisabledMatcher(DateMatcher matcher);
    DatePicker* Presets(const DateRangePreset* values, int count,
                        Listener onSelect = {});
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

template <>
struct EventEmitter<component::DatePickerState, component::DatePickerEvent> {};

} // namespace gpui
#endif // GPUI_SRC_UI_TIME_H_
