/* Ported from crates/base/src/date_picker.rs.
 *
 * The root binds the same Confirm and Cancel actions a select does, and its
 * two handlers are what separate the pair: Enter only ever opens, and the
 * Cancel handler does not look at `disabled` at all. */

#include "Test.h"

// The chord, resolved in the picker's context, read as what the picker does.
static DatePickerAction ForChord(const char* spec, bool open, bool disabled) {
    DatePickerInitKeys();
    KeyChord c = {};
    utassert(KeyChordParse(Str(spec), &c));
    uint32_t ctx = KeyContextOf(DatePickerContext());
    return DatePickerActionOf(KeymapMatch(c, &ctx, 1).action, open, disabled);
}

static void EnterOnlyOpens() {
    utassert(ForChord("enter", false, false) == DatePickerAction::Open);
    // Already open, Enter does nothing: choosing a date is the calendar's
    // business, not the root's. A select would confirm here.
    utassert(ForChord("enter", true, false) == DatePickerAction::None);
}

static void EscapeOnlyCloses() {
    utassert(ForChord("escape", true, false) == DatePickerAction::Dismiss);
    // Closed, Rust propagates it.
    utassert(ForChord("escape", false, false) == DatePickerAction::None);
}

static void OnlyTheConfirmHandlerChecksDisabled() {
    utassert(ForChord("enter", false, true) == DatePickerAction::None);
    // Rust's Cancel handler has no disabled check, so Escape still closes a
    // disabled picker rather than trapping it open.
    utassert(ForChord("escape", true, true) == DatePickerAction::Dismiss);
}

// on_delete: Delete and Backspace clear the date, open or shut. Rust's
// handler has no disabled check, and neither does this.
static void DeleteClearsTheDate() {
    utassert(ForChord("delete", false, false) == DatePickerAction::Clear);
    utassert(ForChord("backspace", true, false) == DatePickerAction::Clear);
    utassert(ForChord("delete", false, true) == DatePickerAction::Clear);
}

static void OtherKeysAreNotThePickers() {
    utassert(ForChord("down", false, false) == DatePickerAction::None);
    utassert(ForChord("space", true, false) == DatePickerAction::None);
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

static El* FindNamedDp(El* root, const char* name) {
    if (!root) {
        return nullptr;
    }
    if (root->id.s && StrEqI(root->id, Str(name))) {
        return root;
    }
    for (El* c = root->first; c; c = c->next) {
        if (El* hit = FindNamedDp(c, name)) {
            return hit;
        }
    }
    return nullptr;
}

// The trigger, the clear and the popup are `input`, `clean` and `pop` in
// every picker; the picker's own name over them is what tells two of them
// apart, the way GPUI's element id stack does.
static void TwoPickersHaveTwoTriggers() {
    App app;
    Window* win = new Window();
    win->app = &app;
    Arena* a = ArenaNew();
    Ctx cx = {};
    cx.app = &app;
    cx.win = win;
    cx.a = a;

    El* page = Div(a);
    El* one = component::DatePicker::New(&cx)
                  ->Id(StrL("one"))
                  ->Year(2025)
                  ->Month(2)
                  ->Day(10)
                  ->Cleanable()
                  ->IntoEl();
    El* two = component::DatePicker::New(&cx)
                  ->Id(StrL("two"))
                  ->Year(2025)
                  ->Month(2)
                  ->Day(11)
                  ->Cleanable()
                  ->IntoEl();
    page->Child(one)->Child(two);
    IdsCollect(page);

    El* inOne = FindNamedDp(one, "input");
    El* inTwo = FindNamedDp(two, "input");
    utassert(inOne && inTwo);
    if (inOne && inTwo) {
        utassert(inOne->clickId != 0 && inTwo->clickId != 0);
        utassert(inOne->clickId != inTwo->clickId);
        // The trigger is what the keyboard reaches, and it is reached
        // separately in each picker.
        utassert(inOne->style.focusId != inTwo->style.focusId);
    }
    El* clearOne = FindNamedDp(one, "clean");
    El* clearTwo = FindNamedDp(two, "clean");
    utassert(clearOne && clearTwo);
    if (clearOne && clearTwo) {
        utassert(clearOne->clickId != clearTwo->clickId);
    }

    WindowKeyedFree(win);
    ArenaDelete(a);
    delete win;
    EntityDropAll(&app);
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
    TwoPickersHaveTwoTriggers();
}
