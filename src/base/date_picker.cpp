#include "base/date_picker.h"
#include "base/actions.h"
#include "gpui/keymap.h"

namespace gpui {

static bool DateValid(LocalDate date) {
    return date.year != 0 && date.month != 0 && date.day != 0;
}

static int DateCompare(LocalDate a, LocalDate b) {
    if (a.year != b.year) {
        return a.year < b.year ? -1 : 1;
    }
    if (a.month != b.month) {
        return a.month < b.month ? -1 : 1;
    }
    if (a.day != b.day) {
        return a.day < b.day ? -1 : 1;
    }
    return 0;
}

// Sakamoto, Sunday = 0, like chrono::Weekday::num_days_from_sunday.
static int DateWeekday(LocalDate date) {
    static const int offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int year = date.year;
    if (date.month < 3) {
        year--;
    }
    return (year + year / 4 - year / 100 + year / 400 +
            offsets[date.month - 1] + date.day) %
           7;
}

DateMatcher DateMatcherWeekdays(uint8_t weekdayMask) {
    DateMatcher matcher;
    matcher.kind = DateMatcherKind::Weekdays;
    matcher.weekdayMask = weekdayMask;
    return matcher;
}

DateMatcher DateMatcherInterval(LocalDate before, LocalDate after) {
    DateMatcher matcher;
    matcher.kind = DateMatcherKind::Interval;
    matcher.from = before;
    matcher.to = after;
    return matcher;
}

DateMatcher DateMatcherRange(LocalDate from, LocalDate to) {
    DateMatcher matcher;
    matcher.kind = DateMatcherKind::Range;
    matcher.from = from;
    matcher.to = to;
    return matcher;
}

DateMatcher DateMatcherCustom(bool (*fn)(LocalDate date)) {
    DateMatcher matcher;
    matcher.kind = DateMatcherKind::Custom;
    matcher.custom = fn;
    return matcher;
}

bool DateMatcherMatches(const DateMatcher& matcher, LocalDate date) {
    switch (matcher.kind) {
        case DateMatcherKind::Weekdays:
            return (matcher.weekdayMask & (1u << DateWeekday(date))) != 0;
        case DateMatcherKind::Interval:
            return (DateValid(matcher.from) &&
                    DateCompare(date, matcher.from) < 0) ||
                   (DateValid(matcher.to) && DateCompare(date, matcher.to) > 0);
        case DateMatcherKind::Range:
            return (!DateValid(matcher.from) ||
                    DateCompare(date, matcher.from) >= 0) &&
                   (!DateValid(matcher.to) ||
                    DateCompare(date, matcher.to) <= 0);
        case DateMatcherKind::Custom:
            return matcher.custom && matcher.custom(date);
        default:
            return false;
    }
}

intptr_t DatePickerDateKey(LocalDate date) {
    return (intptr_t)(date.year * 10000 + date.month * 100 + date.day);
}

LocalDate DatePickerDateFromKey(intptr_t key) {
    int value = (int)key;
    LocalDate date;
    date.year = value / 10000;
    date.month = (value / 100) % 100;
    date.day = value % 100;
    return date;
}

DateSelectionResult DatePickerSelectDate(bool range, LocalDate value,
                                         LocalDate* start, LocalDate* end,
                                         const DateMatcher& disabled) {
    if (DateMatcherMatches(disabled, value)) {
        return DateSelectionResult::Rejected;
    }
    if (!range) {
        *start = value;
        *end = {};
        return DateSelectionResult::Complete;
    }
    if (!DateValid(*start) || DateValid(*end) ||
        DateCompare(value, *start) < 0) {
        *start = value;
        *end = {};
        return DateSelectionResult::Partial;
    }
    *end = value;
    return DateSelectionResult::Complete;
}

Str DatePickerContext() {
    return StrL("DatePicker");
}

void DatePickerInitKeys() {
    static uint32_t bound = 0;
    if (bound == KeymapGeneration()) {
        return;
    }
    bound = KeymapGeneration();
    const char* ctx = "DatePicker";
    // Delete is the input's action, not one of the shared ui:: set — Rust
    // imports it from input:: for exactly these two chords.
    KeyBinding bindings[] = {
        {"enter", action::Confirm(), ctx},
        {"escape", action::Cancel(), ctx},
        {"delete", ActionOf(StrL("input::Delete")), ctx},
        {"backspace", ActionOf(StrL("input::Delete")), ctx},
    };
    KeymapBind(bindings, (int)(sizeof(bindings) / sizeof(bindings[0])));
}

DatePickerAction DatePickerActionOf(uint32_t id, bool open, bool disabled) {
    if (id == action::Confirm()) {
        // Rust's Confirm opens a closed picker and does nothing at all to one
        // that is already open: choosing a date is the calendar's business.
        if (disabled || open) {
            return DatePickerAction::None;
        }
        return DatePickerAction::Open;
    }
    if (id == action::Cancel()) {
        return open ? DatePickerAction::Dismiss : DatePickerAction::None;
    }
    if (id == ActionOf(StrL("input::Delete"))) {
        return DatePickerAction::Clear;
    }
    return DatePickerAction::None;
}

void DatePickerKeys::OnAction(DatePickerKeys* self, Ctx* cx,
                              const ActionEvent* ev) {
    if (!self) {
        return;
    }
    Listener l = {};
    switch (DatePickerActionOf(ev->action, self->open, self->disabled)) {
        case DatePickerAction::Open:
        case DatePickerAction::Dismiss:
            // Both are the toggle the trigger carries, one way each.
            l = self->onToggle;
            break;
        case DatePickerAction::Clear:
            l = self->onClear;
            break;
        case DatePickerAction::None:
            const_cast<ActionEvent*>(ev)->propagate = true;
            return;
    }
    if (!l.IsValid()) {
        return;
    }
    ClickEvent click = {};
    ListenerCall(cx->app, cx->win, l, &click);
}

void DatePickerBindKeys(Ctx* cx, El* root, Str name, Listener onToggle,
                        Listener onClear, bool open, bool disabled) {
    if (!cx || !root) {
        return;
    }
    DatePickerInitKeys();
    Entity<DatePickerKeys> keys =
        KeyedEntity<DatePickerKeys>(cx, (uint32_t)HashClickId(name));
    if (DatePickerKeys* k = keys.Get(cx)) {
        k->onToggle = onToggle;
        k->onClear = onClear;
        k->open = open;
        k->disabled = disabled;
    }
    Listener onAction = ListenTo(keys, &DatePickerKeys::OnAction);
    root->KeyContext(DatePickerContext())
        ->OnAction(action::Confirm(), onAction)
        ->OnAction(action::Cancel(), onAction)
        ->OnAction(ActionOf(StrL("input::Delete")), onAction);
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
