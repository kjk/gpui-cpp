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

// on_delete: Delete and Backspace clear the date, open or shut. Rust's
// handler has no disabled check, and neither does this.
static void DeleteClearsTheDate() {
    utassert(DatePickerActionForKey(KeyDelete, false, false) ==
             DatePickerAction::Clear);
    utassert(DatePickerActionForKey(KeyBack, true, false) ==
             DatePickerAction::Clear);
    utassert(DatePickerActionForKey(KeyDelete, false, true) ==
             DatePickerAction::Clear);
}

static void OtherKeysAreNotThePickers() {
    utassert(DatePickerActionForKey(KeyDown, false, false) ==
             DatePickerAction::None);
    utassert(DatePickerActionForKey(KeySpace, true, false) ==
             DatePickerAction::None);
}

static LocalDate D(int year, int month, int day) {
    return {year, month, day};
}

static bool FirstFiveDays(LocalDate date) {
    return date.day <= 5;
}

static void MatchersKeepTheirRustSemantics() {
    DateMatcher weekends = DateMatcherWeekdays((1u << 0) | (1u << 6));
    utassert(DateMatcherMatches(weekends, D(2025, 2, 9))); // Sunday
    utassert(!DateMatcherMatches(weekends, D(2025, 2, 10)));

    DateMatcher interval = DateMatcherInterval(D(2025, 2, 10), D(2025, 2, 15));
    utassert(DateMatcherMatches(interval, D(2025, 2, 9)));
    utassert(!DateMatcherMatches(interval, D(2025, 2, 12)));
    utassert(DateMatcherMatches(interval, D(2025, 2, 16)));

    DateMatcher range = DateMatcherRange(D(2025, 2, 10), D(2025, 2, 15));
    utassert(!DateMatcherMatches(range, D(2025, 2, 9)));
    utassert(DateMatcherMatches(range, D(2025, 2, 12)));
    utassert(!DateMatcherMatches(range, D(2025, 2, 16)));

    DateMatcher first = DateMatcherCustom(&FirstFiveDays);
    utassert(DateMatcherMatches(first, D(2025, 2, 5)));
    utassert(!DateMatcherMatches(first, D(2025, 2, 6)));
}

static void RangeSelectionRestartsAndCompletes() {
    LocalDate start = {};
    LocalDate end = {};
    DateMatcher none;
    utassert(DatePickerSelectDate(true, D(2025, 2, 10), &start, &end, none) ==
             DateSelectionResult::Partial);
    utassert(DatePickerSelectDate(true, D(2025, 2, 12), &start, &end, none) ==
             DateSelectionResult::Complete);
    utassert(start.day == 10 && end.day == 12);
    utassert(DatePickerSelectDate(true, D(2025, 2, 11), &start, &end, none) ==
             DateSelectionResult::Partial);
    utassert(start.day == 11 && end.day == 0);

    start = D(2025, 2, 12);
    utassert(DatePickerSelectDate(true, D(2025, 2, 10), &start, &end, none) ==
             DateSelectionResult::Partial);
    utassert(start.day == 10 && end.day == 0);

    DateMatcher disabled = DateMatcherRange(D(2025, 2, 1), D(2025, 2, 28));
    utassert(DatePickerSelectDate(false, D(2025, 2, 15), &start, &end,
                                  disabled) == DateSelectionResult::Rejected);
}

void TestDatePicker() {
    TestSuite("date_picker");
    EnterOnlyOpens();
    EscapeOnlyCloses();
    OnlyTheConfirmHandlerChecksDisabled();
    DeleteClearsTheDate();
    OtherKeysAreNotThePickers();
    MatchersKeepTheirRustSemantics();
    RangeSelectionRestartsAndCompletes();
}
