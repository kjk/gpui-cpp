/* Ported from crates/base/src/date_picker.rs.
 *
 * The root binds the same Confirm and Cancel actions a select does, and its
 * two handlers are what separate the pair: Enter only ever opens, and the
 * Cancel handler does not look at `disabled` at all. */

#include "Test.h"

static void EnterOnlyOpens() {
    utassert(DatePickerActionForKey(KeyReturn, false, false) ==
             DatePickerAction::Open);
    // Already open, Enter does nothing: choosing a date is the calendar's
    // business, not the root's. A select would confirm here.
    utassert(DatePickerActionForKey(KeyReturn, true, false) ==
             DatePickerAction::None);
}

static void EscapeOnlyCloses() {
    utassert(DatePickerActionForKey(KeyEscape, true, false) ==
             DatePickerAction::Dismiss);
    // Closed, Rust propagates it.
    utassert(DatePickerActionForKey(KeyEscape, false, false) ==
             DatePickerAction::None);
}

static void OnlyTheConfirmHandlerChecksDisabled() {
    utassert(DatePickerActionForKey(KeyReturn, false, true) ==
             DatePickerAction::None);
    // Rust's Cancel handler has no disabled check, so Escape still closes a
    // disabled picker rather than trapping it open.
    utassert(DatePickerActionForKey(KeyEscape, true, true) ==
             DatePickerAction::Dismiss);
}

static void OtherKeysAreNotThePickers() {
    utassert(DatePickerActionForKey(KeyDown, false, false) ==
             DatePickerAction::None);
    utassert(DatePickerActionForKey(KeySpace, true, false) ==
             DatePickerAction::None);
}

void TestDatePicker() {
    TestSuite("date_picker");
    EnterOnlyOpens();
    EscapeOnlyCloses();
    OnlyTheConfirmHandlerChecksDisabled();
    OtherKeysAreNotThePickers();
}
