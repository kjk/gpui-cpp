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
    Dismiss
};

// The two handlers, whole. Note that only the Confirm one checks `disabled`:
// Rust's Cancel handler does not, so Escape still closes a disabled picker
// that somehow got opened rather than trapping it that way.
DatePickerAction DatePickerActionForKey(int key, bool open, bool disabled);

// The picker takes focus even when disabled — Rust tracks the handle either
// way and only drops it out of tab traversal, so a click still lands on it.
struct DatePicker {
    static El* New(Ctx* cx, Str id, bool disabled = false);
};
} // namespace gpui
