#include "ui/i18n.h"
#include "ui/time.h"
#include "ui/button.h"

namespace gpui {

namespace component {

Calendar* Calendar::New(Ctx* cx) {
    Arena* a = cx->a;
    Calendar* c = ArenaNew<Calendar>(a);
    c->a = a;
    c->cx = cx;
    return c;
}
Calendar* Calendar::Year(int y) {
    year = y;
    return this;
}
Calendar* Calendar::Month(int m) {
    month = m;
    return this;
}
Calendar* Calendar::Day(int d) {
    day = d;
    return this;
}
Calendar* Calendar::Selection(int y, int m, int d) {
    selectedYear = y;
    selectedMonth = m;
    day = d;
    return this;
}
Calendar* Calendar::RangeEnd(int y, int m, int d) {
    rangeEnd = {y, m, d};
    return this;
}
Calendar* Calendar::WithSize(UiSize s) {
    size = s;
    return this;
}
Calendar* Calendar::NumberOfMonths(int count) {
    numberOfMonths = std::max(1, count);
    return this;
}
Calendar* Calendar::View(CalendarView value) {
    view = value;
    return this;
}
Calendar* Calendar::YearRange(int minYear, int maxYear, int pageStart) {
    yearMin = minYear;
    yearMax = maxYear;
    yearPageStart = pageStart;
    return this;
}
Calendar* Calendar::DisabledMatcher(DateMatcher matcher) {
    disabledMatcher = matcher;
    return this;
}
Calendar* Calendar::Bare() {
    bare = true;
    return this;
}
Calendar* Calendar::OnDay(Listener fn) {
    onDay = fn;
    return this;
}
Calendar* Calendar::OnDate(Listener fn) {
    onDate = fn;
    return this;
}
Calendar* Calendar::OnPrev(Listener fn) {
    onPrev = fn;
    return this;
}
Calendar* Calendar::OnNext(Listener fn) {
    onNext = fn;
    return this;
}
Calendar* Calendar::OnMonthToggle(Listener fn) {
    onMonthToggle = fn;
    return this;
}
Calendar* Calendar::OnYearToggle(Listener fn) {
    onYearToggle = fn;
    return this;
}
Calendar* Calendar::OnMonth(Listener fn) {
    onMonth = fn;
    return this;
}
Calendar* Calendar::OnYear(Listener fn) {
    onYear = fn;
    return this;
}

static float CalendarCellSize(UiSize size) {
    if (size == UiSize::Small) {
        return 28;
    }
    if (size == UiSize::Large) {
        return 40;
    }
    return 32;
}

static float CalendarWidth(UiSize size) {
    // crates/ui/src/time/calendar.rs. These came down with the eight-column
    // fix: a month is seven cells wide now rather than however many the
    // wrapping fitted, so the panel is sized to seven of them plus its
    // padding instead of to what the wrap wanted.
    if (size == UiSize::Small) {
        return 220;
    }
    if (size == UiSize::Large) {
        return 304;
    }
    return 248;
}

// The themed item — crates/ui/src/time/calendar.rs. The calendar builds
// every slot and hands it over; what is left up here is the look and the
// label, which is all a theme has to say about a calendar.
static El* ThemedCalendarItem(void* user, Ctx* cx, El* item,
                              const CalendarItemState& st) {
    Calendar* self = (Calendar*)user;
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    // t!("Calendar.week.N"): the heads are the locale's, so a calendar in
    // Chinese reads 日 一 二 rather than Su Mo Tu.
    static const char* weekdays[] = {"Calendar.week.0", "Calendar.week.1",
                                     "Calendar.week.2", "Calendar.week.3",
                                     "Calendar.week.4", "Calendar.week.5",
                                     "Calendar.week.6"};
    // t!("Calendar.month.January"). One-based, so a month number indexes it.
    static const char* months[] = {
        "",
        "Calendar.month.January",
        "Calendar.month.February",
        "Calendar.month.March",
        "Calendar.month.April",
        "Calendar.month.May",
        "Calendar.month.June",
        "Calendar.month.July",
        "Calendar.month.August",
        "Calendar.month.September",
        "Calendar.month.October",
        "Calendar.month.November",
        "Calendar.month.December",
    };
    float cellSize = CalendarCellSize(self->size);
    switch (st.kind) {
        case CalendarItemKind::Previous:
        case CalendarItemKind::Next: {
            bool on = !st.disabled;
            item->Radius(th.radius)
                ->Child(IconEl(a,
                               st.kind == CalendarItemKind::Previous
                                   ? IconName::ChevronLeft
                                   : IconName::ChevronRight,
                               16)
                            ->Fg(on ? th.foreground : th.mutedFg));
            if (on) {
                item->HoverBg(th.secondaryHover)
                    ->FocusId(HashClickId(st.kind == CalendarItemKind::Previous
                                              ? StrL("cal-prev")
                                              : StrL("cal-next")));
            }
            return item;
        }
        case CalendarItemKind::MonthToggle:
        case CalendarItemKind::YearToggle: {
            bool isMonth = st.kind == CalendarItemKind::MonthToggle;
            // Several months at once are plain labels rather than toggles, and
            // the calendar says so by handing over a slot with no id.
            if (self->numberOfMonths > 1) {
                return item
                    ->Child(TextEl(a, isMonth ? Tr(months[st.value])
                                              : StrDup(a, fmt("%d", st.value)))
                                ->Font(14)
                                ->Semibold()
                                ->Fg(th.foreground));
            }
            item->Radius(th.radius);
            if (st.active) {
                item->Bg(th.tokens.primary);
            }
            item->FocusId(HashClickId(isMonth ? StrL("cal-month-toggle")
                                              : StrL("cal-year-toggle")));
            return item
                ->Child(TextEl(a, isMonth ? Tr(months[st.value])
                                          : StrDup(a, fmt("%d", st.value)))
                            ->Font(14)
                            ->Semibold()
                            ->Fg(st.active ? th.primaryFg : th.foreground));
        }
        case CalendarItemKind::Weekday:
            return item->Child(
                TextEl(a, Tr(weekdays[st.value]))->Font(12)->Fg(th.mutedFg));
        case CalendarItemKind::Day: {
            item->Radius(self->size == UiSize::Small   ? th.radius * 0.5f
                         : self->size == UiSize::Large ? th.radius * 2.f
                                                       : th.radius);
            Rgba fg = st.muted ? th.mutedFg : th.foreground;
            if (st.disabled) {
                // calendar.rs fades the whole cell rather than the ink, so a
                // day that is both picked and blocked shows its primary square
                // at half strength.
                item->Opacity(0.5f);
            }
            if (st.active) {
                item->Bg(th.tokens.primary);
                fg = th.primaryFg;
            } else if (st.inRange || st.today) {
                item->Bg(th.tokens.accent);
                fg = th.foreground;
            } else if (!st.disabled) {
                item->HoverBg(th.secondaryHover);
            }
            return item->Child(
                TextEl(a, StrDup(a, fmt("%d", st.value)))->Font(14)->Fg(fg));
        }
        case CalendarItemKind::Month:
            item->Radius(th.radius)->HoverBg(th.secondaryHover);
            if (st.active) {
                item->Bg(th.tokens.primary);
            }
            // `uses_compact_text`: a month option and nothing else, because
            // "September" is what overflows.
            return item
                ->Child(TextEl(a, Tr(months[st.value]))
                            ->Font(12)
                            ->Fg(st.active ? th.primaryFg : th.foreground));
        case CalendarItemKind::Year:
            item->Radius(th.radius)->HoverBg(th.secondaryHover);
            if (st.active) {
                item->Bg(th.tokens.primary);
            }
            return item
                ->Child(TextEl(a, StrDup(a, fmt("%d", st.value)))
                            ->Font(14)
                            ->Fg(st.active ? th.primaryFg : th.foreground));
    }
    (void)cellSize;
    return item;
}

El* Calendar::IntoEl() {
    const Theme& th = cx->theme();
    // date_picker.rs sizes the calendar in its popup itself — 196 / 224 / 280
    // a month — because that one is built `border_0().rounded_none().p_0()`
    // and so has none of the padding the panel width above counts in. The
    // three pairs differ by exactly the 12 either side.
    float width = (CalendarWidth(size) - (bare ? 24.f : 0.f)) * numberOfMonths;
    // The calendar itself is the base one; what is set here is what it is
    // looking at, and the item function that gives it a look.
    CalendarOpts o;
    o.year = year;
    o.month = month;
    o.numberOfMonths = numberOfMonths;
    o.view = view;
    o.cellSize = CalendarCellSize(size);
    o.selected = {selectedYear ? selectedYear : year,
                  selectedMonth ? selectedMonth : month, day};
    o.rangeEnd = rangeEnd;
    o.today = DateToday();
    o.disabledMatcher = disabledMatcher;
    o.yearMin = yearMin;
    o.yearMax = yearMax;
    o.yearPageStart = yearPageStart;
    o.onDay = onDay;
    o.onDate = onDate;
    o.onPrev = onPrev;
    o.onNext = onNext;
    o.onMonthToggle = onMonthToggle;
    o.onYearToggle = onYearToggle;
    o.onMonth = onMonth;
    o.onYear = onYear;
    o.item = &ThemedCalendarItem;
    o.user = this;
    El* root = gpui::Calendar::New(cx, StrL("calendar"), o)->W(width);
    if (!bare) {
        root->Pad(12)->Border(1, th.border)->Radius(th.radiusLg);
    }
    return root;
}

DatePicker* DatePicker::New(Ctx* cx) {
    Arena* a = cx->a;
    DatePicker* d = ArenaNew<DatePicker>(a);
    d->a = a;
    d->cx = cx;
    d->id = StrL("date-picker");
    return d;
}
DatePicker* DatePicker::Id(Str value) {
    id = value;
    return this;
}
DatePicker* DatePicker::Year(int y) {
    year = y;
    return this;
}
DatePicker* DatePicker::Month(int m) {
    month = m;
    return this;
}
DatePicker* DatePicker::Day(int d) {
    day = d;
    return this;
}
DatePicker* DatePicker::View(int y, int m) {
    viewYear = y;
    viewMonth = m;
    return this;
}
DatePicker* DatePicker::RangeEnd(int y, int m, int d) {
    year2 = y;
    month2 = m;
    day2 = d;
    return this;
}
DatePicker* DatePicker::Format(DateFormat f) {
    format = f;
    return this;
}
DatePicker* DatePicker::WithSize(UiSize s) {
    size = s;
    return this;
}
DatePicker* DatePicker::W(float v) {
    width = v;
    return this;
}
DatePicker* DatePicker::Cleanable(bool v) {
    cleanable = v;
    return this;
}
DatePicker* DatePicker::Appearance(bool v) {
    appearance = v;
    return this;
}
DatePicker* DatePicker::FocusRing(bool v) {
    focusRing = v;
    return this;
}
DatePicker* DatePicker::Range(bool v) {
    range = v;
    return this;
}
DatePicker* DatePicker::NumberOfMonths(int count) {
    numberOfMonths = std::max(1, count);
    return this;
}
DatePicker* DatePicker::CalendarMode(CalendarView value) {
    calendarView = value;
    return this;
}
DatePicker* DatePicker::YearRange(int minYear, int maxYear, int pageStart) {
    yearMin = minYear;
    yearMax = maxYear;
    yearPageStart = pageStart;
    return this;
}
DatePicker* DatePicker::DisabledMatcher(DateMatcher matcher) {
    disabledMatcher = matcher;
    return this;
}
DatePicker* DatePicker::Presets(const DateRangePreset* values, int count,
                                Listener onSelect) {
    presets = values;
    presetsCount = count;
    onPreset = onSelect;
    return this;
}
DatePicker* DatePicker::OnClear(Listener fn) {
    onClear = fn;
    return this;
}
DatePicker* DatePicker::Placeholder(Str s) {
    placeholder = s;
    return this;
}
DatePicker* DatePicker::Open(bool v) {
    open = v;
    return this;
}
DatePicker* DatePicker::OnToggle(Listener fn) {
    onToggle = fn;
    return this;
}
DatePicker* DatePicker::OnDay(Listener fn) {
    onDay = fn;
    return this;
}
DatePicker* DatePicker::OnDate(Listener fn) {
    onDate = fn;
    return this;
}
DatePicker* DatePicker::OnPrev(Listener fn) {
    onPrev = fn;
    return this;
}
DatePicker* DatePicker::OnNext(Listener fn) {
    onNext = fn;
    return this;
}
DatePicker* DatePicker::OnMonthToggle(Listener fn) {
    onMonthToggle = fn;
    return this;
}
DatePicker* DatePicker::OnYearToggle(Listener fn) {
    onYearToggle = fn;
    return this;
}
DatePicker* DatePicker::OnMonth(Listener fn) {
    onMonth = fn;
    return this;
}
DatePicker* DatePicker::OnYear(Listener fn) {
    onYear = fn;
    return this;
}

static Str FormatDate(Arena* a, DateFormat f, int y, int m, int d) {
    const char* sep = f == DateFormat::Dash ? "-" : "/";
    return StrDup(a, fmt("%d%s%02d%s%02d", y, Str(sep), m, Str(sep), d));
}

El* DatePicker::IntoEl() {
    const Theme& th = cx->theme();
    bool hasDate = day > 0;
    bool rangeMode = range || year2 > 0;
    bool complete = hasDate && (!rangeMode || day2 > 0);
    Str title;
    if (!complete) {
        title = placeholder.s ? placeholder : Tr("DatePicker.placeholder");
    } else if (rangeMode) {
        title =
            StrDup(a, fmt("%s - %s", FormatDate(a, format, year, month, day),
                          FormatDate(a, format, year2, month2, day2)));
    } else {
        title = FormatDate(a, format, year, month, day);
    }
    // The trigger is input-shaped: the date (or placeholder) with a calendar
    // icon, or the clear button when there is something to clear.
    float height = 32, padX = 10, font = 14;
    if (size == UiSize::Large) {
        height = 44;
        padX = 12;
        font = 16;
    } else if (size == UiSize::Small) {
        height = 24;
        padX = 8;
    } else if (size == UiSize::XSmall) {
        height = 20;
        padX = 4;
        font = 12;
    }
    El* trigger = Div(a)
                      ->FlexRow()
                      ->W(width)
                      ->H(height)
                      ->PadX(padX)
                      ->Gap(4)
                      ->ItemsCenter()
                      ->JustifyBetween();
    if (appearance) {
        trigger->Radius(th.radius)
            ->Bg(th.inputBg)
            ->Border(1, open ? th.ring : th.inputBorder);
    }
    trigger->Child(TextEl(a, title)->Font(font)->Fg(complete ? th.foreground
                                                             : th.mutedFg));
    if (cleanable && hasDate) {
        trigger->Child(Button::New(cx, StrDup(a, fmt("%s-clean", id)))
                           ->Text()
                           ->WithSize(UiSize::XSmall)
                           ->Icon(IconName::X)
                           ->OnClick(onClear)
                           ->IntoEl()
                           ->StopClick());
    } else {
        trigger->Child(IconEl(a, IconName::Calendar, 12)->Fg(th.mutedFg));
    }
    if (!open) {
        BindClick(trigger, StrDup(a, fmt("%s-input", id)), onToggle);
        trigger->FocusRing(focusRing);
    } else {
        // The trigger stops taking the press while the calendar is up — the
        // popup's own mouse-out is what closes it — but it keeps the focus
        // handle it was given when it was pressed. Rust's track_focus is
        // unconditional for the same reason: the picker's key context has to
        // stay over whatever has the focus, or escape belongs to nobody.
        trigger->FocusId(HashClickId(StrDup(a, fmt("%s-input", id))))
            ->FocusRing(false);
    }
    El* popup = nullptr;
    if (open) {
        Calendar* calendar = Calendar::New(cx)
                                 ->Year(viewYear ? viewYear : year)
                                 ->Month(viewMonth ? viewMonth : month)
                                 ->Selection(year, month, day)
                                 ->RangeEnd(year2, month2, day2)
                                 ->WithSize(size)
                                 ->NumberOfMonths(numberOfMonths)
                                 ->View(calendarView)
                                 ->YearRange(yearMin, yearMax, yearPageStart)
                                 ->DisabledMatcher(disabledMatcher)
                                 ->Bare()
                                 ->OnDay(onDay)
                                 ->OnDate(onDate)
                                 ->OnPrev(onPrev)
                                 ->OnNext(onNext)
                                 ->OnMonthToggle(onMonthToggle)
                                 ->OnYearToggle(onYearToggle)
                                 ->OnMonth(onMonth)
                                 ->OnYear(onYear);
        El* content = Div(a)->FlexRow()->Gap(12)->ItemsStart();
        if (presets && presetsCount > 0) {
            El* list = Div(a)->FlexCol()->Gap(8)->PadY(4)->JustifyEnd();
            for (int i = 0; i < presetsCount; i++) {
                const DateRangePreset& preset = presets[i];
                list->Child(component::Button::New(
                                cx, StrDup(a, fmt("date-preset-%d", i)))
                                ->WithSize(UiSize::Small)
                                ->Ghost()
                                ->Label(preset.label)
                                ->OnClick(ListenerArg(onPreset, preset.arg))
                                ->IntoEl());
            }
            content->Child(list);
        }
        content->Child(calendar->IntoEl());
        popup = Div(a)
                    ->Pad(12)
                    ->Border(1, th.border)
                    ->Radius(std::min(th.radius * 2.f, 8.f))
                    ->Bg(th.tokens.background)
                    ->Fg(th.foreground)
                    ->OnMouseUpOut(onToggle)
                    ->Child(content);
    }
    // Both halves of the picker take focus — the outer element and the
    // trigger inside it — so the opt-out has to reach both.
    El* root = gpui::DatePicker::New(cx, id)
                   ->FocusRing(focusRing)
                   ->W(width)
                   ->Child(Popup::New(cx, StrDup(a, fmt("%s-pop", id)), trigger)
                               ->Content(popup)
                               ->IntoEl());
    // date_picker.rs::init binds enter, escape and the two delete keys in the
    // picker's context; the toggle and the clear the caller gave are what
    // they run, which is what Rust's on_action handlers reach for too.
    DatePickerBindKeys(cx, root, id, onToggle, onClear, open, false);
    return root;
}

} // namespace component
} // namespace gpui
