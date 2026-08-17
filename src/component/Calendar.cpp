#include "component/Calendar.h"

namespace component {

struct DayBind {
    Func1<int> fn;
    int day = 1;
};
static void FireDay(DayBind* b) {
    b->fn.Call(b->day);
}

Calendar* Calendar::New(Arena* a) {
    Calendar* c = ::New<Calendar>(a);
    c->a = a;
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
Calendar* Calendar::OnDay(Func1<int> fn) {
    onDay = fn;
    return this;
}
Calendar* Calendar::OnPrev(Func0 fn) {
    onPrev = fn;
    return this;
}
Calendar* Calendar::OnNext(Func0 fn) {
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

El* Calendar::IntoEl() {
    const Theme& th = ThemeNow();
    static const char* mon[] = {"", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    El* root = ::Calendar::New(a, StrL("calendar"))->FlexCol()->W(250)->Pad(12)->Border(1, th.border);
    El* nav = Div(a)->FlexRow()->JustifyBetween()->ItemsCenter();
    El* prev = Div(a)->W(28)->H(28)->ItemsCenter()->JustifyCenter()->Child(IconEl(a, IconName::ChevronRight, 14)->Fg(th.foreground));
    // reuse chevron: flip conceptually — use ChevronRight for next, we'll draw left as text
    prev->Child(TextEl(a, StrL("‹"))->Font(16)->Fg(th.foreground));
    BindClick(prev, StrL("cal-prev"), onPrev);
    El* next = Div(a)->W(28)->H(28)->ItemsCenter()->JustifyCenter()->Child(TextEl(a, StrL("›"))->Font(16)->Fg(th.foreground));
    BindClick(next, StrL("cal-next"), onNext);
    nav->Child(prev)->Child(TextEl(a, str::Dup(a, fmt("%s %d", Str(mon[month]), year)))->Font(13)->Fg(th.foreground))->Child(next);
    root->Child(nav);
    int dim = Dim(year, month);
    El* grid = Div(a)->FlexCol();
    El* row = nullptr;
    for (int d = 1; d <= dim; d++) {
        if ((d - 1) % 7 == 0) {
            row = Div(a)->FlexRow();
            grid->Child(row);
        }
        bool on = d == day;
        El* cell = CalendarItem::New(a, HashClickId(str::Dup(a, fmt("d%d", d))))
                       ->W(32)
                       ->H(32)
                       ->ItemsCenter()
                       ->JustifyCenter();
        if (on) {
            cell->Bg(th.primary)->Child(TextEl(a, str::Dup(a, fmt("%d", d)))->Font(12)->Fg(th.primaryFg));
        } else {
            cell->Child(TextEl(a, str::Dup(a, fmt("%d", d)))->Font(12)->Fg(th.foreground));
        }
        if (onDay.IsValid()) {
            DayBind* b = ::New<DayBind>(a);
            b->fn = onDay;
            b->day = d;
            cell->OnClick(MkFunc0(&FireDay, b));
        }
        row->Child(cell);
    }
    root->Child(grid);
    return root;
}

} // namespace component
