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

El* CalendarItem::New(Ctx* cx, Str id, Listener onClick) {
    Arena* a = cx->a;
    El* e = Div(a);
    if (id.s) {
        e->Id(id)->Click(HashClickId(id));
    }
    if (onClick.IsValid()) {
        e->OnClick(onClick);
    }
    return e;
}
} // namespace gpui
