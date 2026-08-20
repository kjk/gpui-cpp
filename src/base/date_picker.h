/* Unstyled date picker — crates/base/src/date_picker.rs */

#include "gpui/gpui.h"

namespace gpui {

// What a keystroke asks a date picker to do. Rust binds the same Confirm and
// Cancel actions a select does, but its handlers are not the select's: Enter
// only opens, and does nothing at all to a picker that is already open —
// choosing a date is the calendar's business, not the root's.
enum class DatePickerAction : uint8_t {
    None,
    Open,
    Dismiss,
    // Delete or Backspace: `on_delete`, which is the clear button's handler
    // reached from the keyboard.
    Clear
};

// The three handlers, whole. Note that only the Confirm one checks `disabled`:
// neither Rust's Cancel nor its Delete does, so Escape still closes a disabled
// picker that somehow got opened rather than trapping it that way.
DatePickerAction DatePickerActionForKey(int key, bool open, bool disabled);

// Calendar::Matcher, kept POD-friendly so a picker can copy it into its frame
// element. Range disables dates inside its bounds; Interval disables dates
// outside its bounds, matching the two easily-confused Rust variants.
enum class DateMatcherKind : uint8_t {
    None,
    Weekdays,
    Interval,
    Range,
    Custom
};

struct DateMatcher {
    DateMatcherKind kind = DateMatcherKind::None;
    uint8_t weekdayMask = 0;
    LocalDate from = {};
    LocalDate to = {};
    bool (*custom)(LocalDate date) = nullptr;
};

DateMatcher DateMatcherWeekdays(uint8_t weekdayMask);
DateMatcher DateMatcherInterval(LocalDate before, LocalDate after);
DateMatcher DateMatcherRange(LocalDate from, LocalDate to);
DateMatcher DateMatcherCustom(bool (*fn)(LocalDate date));
bool DateMatcherMatches(const DateMatcher& matcher, LocalDate date);
intptr_t DatePickerDateKey(LocalDate date);
LocalDate DatePickerDateFromKey(intptr_t key);

enum class DateSelectionResult : uint8_t {
    Rejected,
    Partial,
    Complete
};

// CalendarState::select_date. A complete range restarts on the next click; an
// earlier second endpoint also restarts. Only a complete value is emitted by
// Rust, which is why Partial and Complete are distinct here.
DateSelectionResult DatePickerSelectDate(bool range, LocalDate value,
                                         LocalDate* start, LocalDate* end,
                                         const DateMatcher& disabled);

// The picker takes focus even when disabled — Rust tracks the handle either
// way and only drops it out of tab traversal, so a click still lands on it.
struct DatePicker {
    static El* New(Ctx* cx, Str id, bool disabled = false);
};
} // namespace gpui
