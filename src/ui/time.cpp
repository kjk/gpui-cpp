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

static int Dim(int y, int m) {
    static const int k[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) {
        return 29;
    }
    return k[m];
}

// Sakamoto: 0 = Sunday. The grid starts on the weekday the 1st falls on and
// fills the flanks with the neighbouring months, as crates/ui does.
static int Dow(int y, int m, int d) {
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) {
        y -= 1;
    }
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

static void OffsetMonth(int year, int month, int offset, int* outYear,
                        int* outMonth) {
    int value = month - 1 + offset;
    *outYear = year + value / 12;
    *outMonth = value % 12 + 1;
}

static bool SameDate(LocalDate a, LocalDate b) {
    return a.year == b.year && a.month == b.month && a.day == b.day;
}

static bool DateAtOrBefore(LocalDate a, LocalDate b) {
    return DatePickerDateKey(a) <= DatePickerDateKey(b);
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
    if (size == UiSize::Small) {
        return 232;
    }
    if (size == UiSize::Large) {
        return 316;
    }
    return 288;
}

static El* CalendarMonth(Calendar* self, int year, int month, float cellSize) {
    Arena* a = self->a;
    Ctx* cx = self->cx;
    const Theme& th = cx->theme();
    static const char* weekdays[] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
    El* panel = Div(a)->FlexCol();
    El* head = Div(a)->FlexRow();
    for (int i = 0; i < 7; i++) {
        head->Child(
            Div(a)
                ->W(cellSize)
                ->H(cellSize)
                ->ItemsCenter()
                ->JustifyCenter()
                ->Child(TextEl(a, Str(weekdays[i]))->Font(12)->Fg(th.mutedFg)));
    }
    panel->Child(head);

    LocalDate today = DateToday();
    LocalDate selected = {
        self->selectedYear ? self->selectedYear : self->year,
        self->selectedMonth ? self->selectedMonth : self->month,
        self->day,
    };
    int offset = CalendarGridOffset(Dow(year, month, 1));
    int cells = CalendarGridCells(offset, Dim(year, month));
    LocalDate first = DateAddDays({year, month, 1}, -offset);
    El* grid = Div(a)->FlexCol()->Gap(2);
    for (int rowIx = 0; rowIx < cells / 7; rowIx++) {
        El* row = Div(a)->FlexRow();
        for (int col = 0; col < 7; col++) {
            LocalDate date = DateAddDays(first, rowIx * 7 + col);
            bool outside = date.month != month;
            bool disabled = DateMatcherMatches(self->disabledMatcher, date);
            bool muted = outside || disabled;
            bool active =
                SameDate(date, selected) || SameDate(date, self->rangeEnd);
            bool inRange = self->day > 0 && self->rangeEnd.day > 0 &&
                           DateAtOrBefore(selected, date) &&
                           DateAtOrBefore(date, self->rangeEnd);
            bool isToday = SameDate(date, today);
            El* cell =
                CalendarItem::New(cx,
                                  StrDup(a, fmt("date-%d-%02d-%02d", date.year,
                                                date.month, date.day)))
                    ->W(cellSize)
                    ->H(cellSize)
                    ->ItemsCenter()
                    ->JustifyCenter()
                    ->Radius(self->size == UiSize::Small   ? th.radius * 0.5f
                             : self->size == UiSize::Large ? th.radius * 2.f
                                                           : th.radius);
            Rgba fg = muted ? th.mutedFg : th.foreground;
            if (disabled) {
                fg = RgbaOpacity(fg, 0.5f);
            }
            if (active) {
                cell->Bg(th.primary);
                fg = th.primaryFg;
            } else if (inRange || isToday) {
                cell->Bg(th.accent);
                fg = th.foreground;
            } else if (!disabled) {
                cell->HoverBg(th.secondaryHover);
            }
            cell->Child(
                TextEl(a, StrDup(a, fmt("%d", date.day)))->Font(14)->Fg(fg));
            if (!disabled) {
                if (self->onDate.IsValid()) {
                    cell->OnClick(
                        ListenerArg(self->onDate, DatePickerDateKey(date)));
                } else if (!outside && self->onDay.IsValid()) {
                    cell->OnClick(ListenerArg(self->onDay, date.day));
                }
            }
            row->Child(cell);
        }
        grid->Child(row);
    }
    panel->Child(grid);
    return panel;
}

El* Calendar::IntoEl() {
    const Theme& th = cx->theme();
    static const char* months[] = {
        "",        "January",  "February", "March",  "April",
        "May",     "June",     "July",     "August", "September",
        "October", "November", "December",
    };
    float cellSize = CalendarCellSize(size);
    float width = CalendarWidth(size) * numberOfMonths;
    El* root = gpui::Calendar::New(cx, StrL("calendar"))
                   ->FlexCol()
                   ->W(width)
                   ->Pad(12)
                   ->Gap(2)
                   ->Border(1, th.border)
                   ->Radius(th.radiusLg);

    El* nav = Div(a)->FlexRow()->W(kFill)->JustifyBetween()->ItemsCenter();
    bool canPrev = view == CalendarView::Day ||
                   (view == CalendarView::Year && yearPageStart > yearMin);
    El* prev = Div(a)
                   ->W(cellSize)
                   ->H(cellSize)
                   ->ItemsCenter()
                   ->JustifyCenter()
                   ->Radius(th.radius)
                   ->Child(IconEl(a, IconName::ChevronLeft, 16)
                               ->Fg(canPrev ? th.foreground : th.mutedFg));
    if (canPrev) {
        prev->HoverBg(th.secondaryHover);
        BindClick(prev, StrL("cal-prev"), onPrev);
    }
    El* labels = Div(a)->FlexRow()->Grow()->ItemsCenter();
    for (int i = 0; i < numberOfMonths; i++) {
        int shownYear = 0, shownMonth = 0;
        OffsetMonth(year, month, i, &shownYear, &shownMonth);
        if (numberOfMonths == 1) {
            El* label = Div(a)
                            ->FlexRow()
                            ->H(cellSize)
                            ->Grow()
                            ->Gap(16)
                            ->ItemsCenter()
                            ->JustifyCenter();
            El* monthLabel = Div(a)
                                 ->H(cellSize)
                                 ->PadX(8)
                                 ->ItemsCenter()
                                 ->Radius(th.radius)
                                 ->Child(TextEl(a, Str(months[shownMonth]))
                                             ->Font(14)
                                             ->Semibold()
                                             ->Fg(view == CalendarView::Month
                                                      ? th.primaryFg
                                                      : th.foreground));
            if (view == CalendarView::Month) {
                monthLabel->Bg(th.primary);
            }
            BindClick(monthLabel, StrL("cal-month-toggle"), onMonthToggle);
            El* yearLabel =
                Div(a)
                    ->H(cellSize)
                    ->PadX(8)
                    ->ItemsCenter()
                    ->Radius(th.radius)
                    ->Child(TextEl(a, StrDup(a, fmt("%d", shownYear)))
                                ->Font(14)
                                ->Semibold()
                                ->Fg(view == CalendarView::Year
                                         ? th.primaryFg
                                         : th.foreground));
            if (view == CalendarView::Year) {
                yearLabel->Bg(th.primary);
            }
            BindClick(yearLabel, StrL("cal-year-toggle"), onYearToggle);
            label->Child(monthLabel)->Child(yearLabel);
            labels->Child(label);
        } else {
            El* label = Div(a)
                            ->FlexCol()
                            ->H(cellSize)
                            ->Grow()
                            ->ItemsCenter()
                            ->JustifyCenter();
            label->Child(TextEl(a, Str(months[shownMonth]))
                             ->Font(14)
                             ->Semibold()
                             ->Fg(th.foreground));
            label->Child(TextEl(a, StrDup(a, fmt("%d", shownYear)))
                             ->Font(14)
                             ->Semibold()
                             ->Fg(th.foreground));
            labels->Child(label);
        }
    }
    bool canNext = view == CalendarView::Day ||
                   (view == CalendarView::Year && yearPageStart + 20 < yearMax);
    El* next = Div(a)
                   ->W(cellSize)
                   ->H(cellSize)
                   ->ItemsCenter()
                   ->JustifyCenter()
                   ->Radius(th.radius)
                   ->Child(IconEl(a, IconName::ChevronRight, 16)
                               ->Fg(canNext ? th.foreground : th.mutedFg));
    if (canNext) {
        next->HoverBg(th.secondaryHover);
        BindClick(next, StrL("cal-next"), onNext);
    }
    nav->Child(prev)->Child(labels)->Child(next);
    root->Child(nav);

    El* body = Div(a)->FlexRow()->W(kFill);
    if (view == CalendarView::Day) {
        body->JustifyBetween()->ItemsStart();
        for (int i = 0; i < numberOfMonths; i++) {
            int shownYear = 0, shownMonth = 0;
            OffsetMonth(year, month, i, &shownYear, &shownMonth);
            body->Child(CalendarMonth(this, shownYear, shownMonth, cellSize));
        }
    } else if (view == CalendarView::Month) {
        body->FlexWrap();
        float itemWidth = (width - 24) / 3.f;
        for (int m = 1; m <= 12; m++) {
            El* item =
                CalendarItem::New(cx, StrDup(a, fmt("calendar-month-%d", m)))
                    ->W(itemWidth)
                    ->H(cellSize)
                    ->ItemsCenter()
                    ->JustifyCenter()
                    ->Radius(th.radius)
                    ->HoverBg(th.secondaryHover)
                    ->Child(
                        TextEl(a, Str(months[m]))
                            ->Font(14)
                            ->Fg(m == month ? th.primaryFg : th.foreground));
            if (m == month) {
                item->Bg(th.primary);
            }
            item->OnClick(ListenerArg(onMonth, m));
            body->Child(item);
        }
    } else {
        body->FlexWrap();
        int minYear = yearMin ? yearMin : year - 50;
        int maxYear = yearMax ? yearMax : year + 50;
        int firstYear = yearPageStart ? yearPageStart : minYear;
        float itemWidth = (width - 24) / 5.f;
        for (int y = firstYear; y < std::min(firstYear + 20, maxYear); y++) {
            El* item =
                CalendarItem::New(cx, StrDup(a, fmt("calendar-year-%d", y)))
                    ->W(itemWidth)
                    ->H(cellSize)
                    ->ItemsCenter()
                    ->JustifyCenter()
                    ->Radius(th.radius)
                    ->HoverBg(th.secondaryHover)
                    ->Child(TextEl(a, StrDup(a, fmt("%d", y)))
                                ->Font(14)
                                ->Fg(y == year ? th.primaryFg : th.foreground));
            if (y == year) {
                item->Bg(th.primary);
            }
            item->OnClick(ListenerArg(onYear, y));
            body->Child(item);
        }
    }
    root->Child(body);
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
        title = placeholder.s ? placeholder : StrL("Select date");
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
                           ->IntoEl());
    } else {
        trigger->Child(IconEl(a, IconName::Calendar, 12)->Fg(th.mutedFg));
    }
    if (!open) {
        BindClick(trigger, StrDup(a, fmt("%s-input", id)), onToggle);
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
                    ->Bg(th.background)
                    ->Fg(th.foreground)
                    ->OnMouseUpOut(onToggle)
                    ->Child(content);
    }
    return gpui::DatePicker::New(cx, id)
        ->W(width)
        ->Child(Popup::New(cx, StrDup(a, fmt("%s-pop", id)), trigger)
                    ->Content(popup)
                    ->IntoEl());
}

} // namespace component
} // namespace gpui
