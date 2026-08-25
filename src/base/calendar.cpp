#include "base/calendar.h"

namespace gpui {

void CalendarPrevMonth(CalendarState* s) {
    if (s->currentMonth == 1) {
        s->currentYear--;
        s->currentMonth = 12;
    } else {
        s->currentMonth--;
    }
}

void CalendarNextMonth(CalendarState* s) {
    if (s->currentMonth == 12) {
        s->currentYear++;
        s->currentMonth = 1;
    } else {
        s->currentMonth++;
    }
}

bool CalendarHasPrevYearPage(const CalendarState* s) {
    return s->yearPage > 0;
}

bool CalendarHasNextYearPage(const CalendarState* s) {
    return s->yearPage < s->yearPageCount - 1;
}

bool CalendarPrevYearPage(CalendarState* s) {
    if (!CalendarHasPrevYearPage(s)) {
        return false;
    }
    s->yearPage--;
    return true;
}

bool CalendarNextYearPage(CalendarState* s) {
    if (!CalendarHasNextYearPage(s)) {
        return false;
    }
    s->yearPage++;
    return true;
}

int CalendarGridOffset(int firstWeekday) {
    // Rust: (weekday - first_day + 7) % 7, with Sunday as first_day.
    return ((firstWeekday % 7) + 7) % 7;
}

int CalendarGridCells(int offset, int daysInMonth) {
    int span = offset + daysInMonth;
    if (span <= 0) {
        return 0;
    }
    return ((span + 6) / 7) * 7;
}

El* Calendar::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return Div(a)->Id(id);
}

// Sakamoto: 0 = Sunday. The grid starts on the weekday the 1st falls on and
// fills the flanks with the neighbouring months.
int CalendarWeekday(int year, int month, int day) {
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (month < 3) {
        year -= 1;
    }
    return (year + year / 4 - year / 100 + year / 400 + t[month - 1] + day) % 7;
}

int CalendarDaysInMonth(int year, int month) {
    static const int k[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) {
        return 29;
    }
    return k[month];
}

void CalendarOffsetMonth(int year, int month, int offset, int* outYear,
                         int* outMonth) {
    int value = month - 1 + offset;
    *outYear = year + value / 12;
    *outMonth = value % 12 + 1;
}

static bool CalSameDate(LocalDate a, LocalDate b) {
    return a.year == b.year && a.month == b.month && a.day == b.day;
}

static bool CalAtOrBefore(LocalDate a, LocalDate b) {
    return DatePickerDateKey(a) <= DatePickerDateKey(b);
}

// One slot: built here, decorated by the caller, and what comes back is what
// goes in the grid.
static El* CalSlot(Ctx* cx, const CalendarOpts& o, Str id,
                   const CalendarItemState& st, Listener onClick) {
    El* item = CalendarItem::New(cx, id, onClick);
    if (!o.item) {
        return item;
    }
    El* built = o.item(o.user, cx, item, st);
    return built ? built : item;
}

// One month's grid: the seven weekday heads, then a row per week. It is a
// column of rows rather than one wrapping row, so seven is seven whatever
// width the panel is given.
static El* CalMonthGrid(Ctx* cx, const CalendarOpts& o, int year, int month) {
    Arena* a = cx->a;
    El* panel = Div(a)->FlexCol();
    El* header = Div(a)->FlexRow();
    for (int i = 0; i < 7; i++) {
        CalendarItemState st;
        st.kind = CalendarItemKind::Weekday;
        st.value = i;
        header->Child(CalSlot(cx, o, {}, st, {})
                          ->W(o.cellSize)
                          ->H(o.cellSize)
                          ->ItemsCenter()
                          ->JustifyCenter());
    }
    panel->Child(header);

    int offset = CalendarGridOffset(CalendarWeekday(year, month, 1));
    int cells = CalendarGridCells(offset, CalendarDaysInMonth(year, month));
    LocalDate first = DateAddDays({year, month, 1}, -offset);
    El* week = nullptr;
    for (int i = 0; i < cells; i++) {
        if (i % 7 == 0) {
            week = Div(a)->FlexRow();
            panel->Child(week);
        }
        LocalDate date = DateAddDays(first, i);
        bool outside = date.month != month;
        CalendarItemState st;
        st.kind = CalendarItemKind::Day;
        st.value = date.day;
        st.date = date;
        st.disabled = DateMatcherMatches(o.disabledMatcher, date);
        st.muted = outside || st.disabled;
        st.active =
            CalSameDate(date, o.selected) || CalSameDate(date, o.rangeEnd);
        st.inRange = o.selected.day > 0 && o.rangeEnd.day > 0 &&
                     CalAtOrBefore(o.selected, date) &&
                     CalAtOrBefore(date, o.rangeEnd);
        st.today = CalSameDate(date, o.today);
        // A blocked day keeps its id and its place and takes no click, which
        // is the same bargain a disabled button strikes.
        Listener click = {};
        if (!st.disabled) {
            if (o.onDate.IsValid()) {
                click = ListenerArg(o.onDate, DatePickerDateKey(date));
            } else if (!outside && o.onDay.IsValid()) {
                click = ListenerArg(o.onDay, date.day);
            }
        }
        Str id = StrDup(
            a, fmt("date-%d-%02d-%02d", date.year, date.month, date.day));
        week->Child(CalSlot(cx, o, id, st, click)
                        ->W(o.cellSize)
                        ->H(o.cellSize)
                        ->ItemsCenter()
                        ->JustifyCenter());
    }
    return panel;
}

El* Calendar::New(Ctx* cx, Str id, const CalendarOpts& o) {
    Arena* a = cx->a;
    El* root = New(cx, id)->FlexCol()->Gap(2);

    // The header: an arrow either side of the month and year the calendar is
    // looking at. An arrow that has nowhere to go is disabled rather than
    // absent, so the header keeps its shape.
    El* nav = Div(a)->FlexRow()->W(kFill)->JustifyBetween()->ItemsCenter();
    bool canPrev =
        o.view == CalendarView::Day ||
        (o.view == CalendarView::Year && o.yearPageStart > o.yearMin);
    CalendarItemState prevSt;
    prevSt.kind = CalendarItemKind::Previous;
    prevSt.disabled = !canPrev;
    nav->Child(CalSlot(cx, o, canPrev ? StrL("cal-prev") : Str{}, prevSt,
                       canPrev ? o.onPrev : Listener{})
                   ->W(o.cellSize)
                   ->H(o.cellSize)
                   ->ItemsCenter()
                   ->JustifyCenter());

    El* labels = Div(a)->FlexRow()->Flex1()->ItemsCenter();
    for (int i = 0; i < o.numberOfMonths; i++) {
        int shownYear = 0, shownMonth = 0;
        CalendarOffsetMonth(o.year, o.month, i, &shownYear, &shownMonth);
        // One month is the pair of toggles that switch the grid below;
        // several are plain labels, since there is one grid per month and
        // nothing to switch.
        if (o.numberOfMonths == 1) {
            El* label = Div(a)
                            ->FlexRow()
                            ->H(o.cellSize)
                            ->Flex1()
                            ->Gap(16)
                            ->ItemsCenter()
                            ->JustifyCenter();
            CalendarItemState mSt;
            mSt.kind = CalendarItemKind::MonthToggle;
            mSt.value = shownMonth;
            mSt.active = o.view == CalendarView::Month;
            label->Child(
                CalSlot(cx, o, StrL("cal-month-toggle"), mSt, o.onMonthToggle)
                    ->H(o.cellSize)
                    ->PadX(8)
                    ->ItemsCenter());
            CalendarItemState ySt;
            ySt.kind = CalendarItemKind::YearToggle;
            ySt.value = shownYear;
            ySt.active = o.view == CalendarView::Year;
            label->Child(
                CalSlot(cx, o, StrL("cal-year-toggle"), ySt, o.onYearToggle)
                    ->H(o.cellSize)
                    ->PadX(8)
                    ->ItemsCenter());
            labels->Child(label);
        } else {
            El* label = Div(a)
                            ->FlexCol()
                            ->H(o.cellSize)
                            ->Flex1()
                            ->ItemsCenter()
                            ->JustifyCenter();
            CalendarItemState mSt;
            mSt.kind = CalendarItemKind::MonthToggle;
            mSt.value = shownMonth;
            CalendarItemState ySt;
            ySt.kind = CalendarItemKind::YearToggle;
            ySt.value = shownYear;
            label->Child(CalSlot(cx, o, {}, mSt, {}));
            label->Child(CalSlot(cx, o, {}, ySt, {}));
            labels->Child(label);
        }
    }
    nav->Child(labels);

    bool canNext =
        o.view == CalendarView::Day ||
        (o.view == CalendarView::Year && o.yearPageStart + 20 < o.yearMax);
    CalendarItemState nextSt;
    nextSt.kind = CalendarItemKind::Next;
    nextSt.disabled = !canNext;
    nav->Child(CalSlot(cx, o, canNext ? StrL("cal-next") : Str{}, nextSt,
                       canNext ? o.onNext : Listener{})
                   ->W(o.cellSize)
                   ->H(o.cellSize)
                   ->ItemsCenter()
                   ->JustifyCenter());
    root->Child(nav);

    El* body = Div(a)->FlexRow()->W(kFill);
    if (o.view == CalendarView::Day) {
        // `h_flex().justify_around()`: the months sit side by side with the
        // space shared out around them.
        body->JustifyAround();
        for (int i = 0; i < o.numberOfMonths; i++) {
            int shownYear = 0, shownMonth = 0;
            CalendarOffsetMonth(o.year, o.month, i, &shownYear, &shownMonth);
            body->Child(CalMonthGrid(cx, o, shownYear, shownMonth));
        }
    } else if (o.view == CalendarView::Month) {
        // `picker_grid_layout`: three columns, 4 between them, and a cell
        // that fills its column. There is no grid in this tree's style, so
        // the same shape is rows of three with the cells grown.
        body->FlexCol()->Gap(8);
        El* row = nullptr;
        for (int m = 1; m <= 12; m++) {
            if ((m - 1) % 3 == 0) {
                row = Div(a)->FlexRow()->W(kFill)->Gap(4);
                body->Child(row);
            }
            CalendarItemState st;
            st.kind = CalendarItemKind::Month;
            st.value = m;
            st.active = m == o.month;
            row->Child(CalSlot(cx, o, StrDup(a, fmt("calendar-month-%d", m)),
                               st, ListenerArg(o.onMonth, m))
                           ->Flex1()
                           ->H(o.cellSize)
                           ->ItemsCenter()
                           ->JustifyCenter());
        }
    } else {
        // Five columns for the years, on the same rule.
        body->FlexCol()->Gap(8);
        int minYear = o.yearMin ? o.yearMin : o.year - 50;
        int maxYear = o.yearMax ? o.yearMax : o.year + 50;
        int firstYear = o.yearPageStart ? o.yearPageStart : minYear;
        int last = firstYear + 20 < maxYear ? firstYear + 20 : maxYear;
        El* row = nullptr;
        int ix = 0;
        for (int y = firstYear; y < last; y++, ix++) {
            if (ix % 5 == 0) {
                row = Div(a)->FlexRow()->W(kFill)->Gap(4);
                body->Child(row);
            }
            CalendarItemState st;
            st.kind = CalendarItemKind::Year;
            st.value = y;
            st.active = y == o.year;
            row->Child(CalSlot(cx, o, StrDup(a, fmt("calendar-year-%d", y)), st,
                               ListenerArg(o.onYear, y))
                           ->Flex1()
                           ->H(o.cellSize)
                           ->ItemsCenter()
                           ->JustifyCenter());
        }
    }
    root->Child(body);
    return root;
}

El* CalendarItem::New(Ctx* cx, Str id, Listener onClick) {
    Arena* a = cx->a;
    El* e = Div(a);
    if (id.s) {
        e->PathClick(id);
    }
    if (onClick.IsValid()) {
        e->OnClick(onClick);
    }
    return e;
}
} // namespace gpui
