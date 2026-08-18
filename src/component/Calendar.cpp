#include "component/Calendar.h"

namespace gpui {

namespace component {

struct DayBind {
    Func1<int> fn;
    int day = 1;
};
static void FireDay(DayBind* b) {
    b->fn.Call(b->day);
}

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
Calendar* Calendar::OnDay(Listener fn) {
    onDay = fn;
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

static void PrevMonth(int y, int m, int* py, int* pm) {
    *pm = m - 1;
    *py = y;
    if (*pm < 1) {
        *pm = 12;
        (*py)--;
    }
}

El* Calendar::IntoEl() {
    const Theme& th = cx->theme();
    static const char* mon[] = {"",        "January",   "February", "March",
                                "April",   "May",       "June",     "July",
                                "August",  "September", "October",  "November",
                                "December"};
    static const char* wd[] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
    // Medium: size_8 cells, w 288, p_3, gap_0p5, rounded radius_lg.
    const float kCell = 32.f;
    El* root = gpui::Calendar::New(cx, StrL("calendar"))
                   ->FlexCol()
                   ->W(288)
                   ->Pad(12)
                   ->Gap(2)
                   ->Border(1, th.border)
                   ->Radius(th.radiusLg);

    // Header: the arrows sit at the edges, the month and year toggles between.
    El* nav = Div(a)->FlexRow()->W(kFill)->JustifyBetween()->ItemsCenter();
    El* prev =
        Div(a)
            ->W(kCell)
            ->H(kCell)
            ->ItemsCenter()
            ->JustifyCenter()
            ->Radius(th.radius)
            ->HoverBg(th.secondaryHover)
            ->Child(IconEl(a, IconName::ChevronLeft, 16)->Fg(th.foreground));
    BindClick(prev, StrL("cal-prev"), onPrev);
    El* next =
        Div(a)
            ->W(kCell)
            ->H(kCell)
            ->ItemsCenter()
            ->JustifyCenter()
            ->Radius(th.radius)
            ->HoverBg(th.secondaryHover)
            ->Child(IconEl(a, IconName::ChevronRight, 16)->Fg(th.foreground));
    BindClick(next, StrL("cal-next"), onNext);
    El* toggles = Div(a)->FlexRow()->Grow()->JustifyCenter()->Gap(16);
    toggles->Child(Div(a)->PadX(8)->H(kCell)->ItemsCenter()->Child(
        TextEl(a, Str(mon[month]))->Font(14)->Semibold()->Fg(th.foreground)));
    toggles->Child(Div(a)->PadX(8)->H(kCell)->ItemsCenter()->Child(
        TextEl(a, StrDup(a, fmt("%d", year)))
            ->Font(14)
            ->Semibold()
            ->Fg(th.foreground)));
    nav->Child(prev)->Child(toggles)->Child(next);
    root->Child(nav);

    // Weekday header, text_xs and muted.
    El* head = Div(a)->FlexRow()->W(kFill);
    for (int i = 0; i < 7; i++) {
        head->Child(
            Div(a)->W(kCell)->H(kCell)->ItemsCenter()->JustifyCenter()->Child(
                TextEl(a, Str(wd[i]))->Font(12)->Fg(th.mutedFg)));
    }
    root->Child(head);

    // Today, for the "current day" ring.
    LocalDate now = DateToday();
    int dim = Dim(year, month);
    int lead = Dow(year, month, 1);
    int prevY = 0, prevM = 0;
    PrevMonth(year, month, &prevY, &prevM);
    int prevDim = Dim(prevY, prevM);

    El* grid = Div(a)->FlexCol()->Gap(2);
    for (int week = 0; week < 6; week++) {
        El* row = Div(a)->FlexRow();
        for (int col = 0; col < 7; col++) {
            int cellIx = week * 7 + col;
            int d = cellIx - lead + 1;
            bool muted = d < 1 || d > dim;
            int shown = d;
            if (d < 1) {
                shown = prevDim + d;
            } else if (d > dim) {
                shown = d - dim;
            }
            bool active = !muted && d == day;
            bool today = !muted && year == now.year && month == now.month &&
                         d == now.day;
            El* cell = CalendarItem::New(
                           cx, HashClickId(StrDup(a, fmt("d%d-%d", month, d))))
                           ->W(kCell)
                           ->H(kCell)
                           ->ItemsCenter()
                           ->JustifyCenter()
                           ->Radius(th.radius);
            Rgba fg = muted ? th.mutedFg : th.foreground;
            if (active) {
                cell->Bg(th.primary);
                fg = th.primaryFg;
            } else if (today) {
                cell->Bg(th.accent);
                fg = th.foreground;
            } else if (!muted) {
                cell->HoverBg(th.secondaryHover);
            }
            cell->Child(
                TextEl(a, StrDup(a, fmt("%d", shown)))->Font(14)->Fg(fg));
            if (!muted && onDay.IsValid()) {
                cell->OnClick(ListenerArg(onDay, d));
            }
            row->Child(cell);
        }
        grid->Child(row);
    }
    root->Child(grid);
    return root;
}

} // namespace component
} // namespace gpui
