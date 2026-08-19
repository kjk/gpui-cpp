#include "base/date_picker.h"

namespace gpui {

DatePickerAction DatePickerActionForKey(int key, bool open, bool disabled) {
    if (key == KeyReturn) {
        // Rust propagates when disabled, and does nothing at all when the
        // picker is already open: the calendar takes it from there.
        if (disabled || open) {
            return DatePickerAction::None;
        }
        return DatePickerAction::Open;
    }
    if (key == KeyEscape) {
        // No disabled check here — Rust's Cancel handler has none either.
        return open ? DatePickerAction::Dismiss : DatePickerAction::None;
    }
    return DatePickerAction::None;
}

El* DatePicker::New(Ctx* cx, Str id, bool disabled) {
    Arena* a = cx->a;
    int clickId = HashClickId(id);
    // track_focus is unconditional in Rust; only tab_stop follows `disabled`,
    // and there is no separate tab-stop flag here, so a disabled picker keeps
    // its identity and gives up the traversal slot.
    El* e = Div(a)->Id(id)->Click(clickId);
    if (!disabled) {
        e->FocusId(clickId);
    }
    return e;
}
} // namespace gpui
