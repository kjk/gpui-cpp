#include "Showcase.h"
#include "ui/Calendar.h"

enum {
    ClickCalPrev = 230,
    ClickCalNext = 231,
    ClickCalDay = 232, // + 0..41
};

static const char* kMon[] = {"", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                             "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
static const char* kWd[] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};

static int DaysInMonth(int y, int m) {
    static const int k[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) {
        return 29;
    }
    return k[m];
}

static int Dow(int y, int m, int d) {
    // Sakamoto: 0 = Sunday. Aug 1 2026 is Saturday.
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) {
        y--;
    }
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

static void EnsureCalendarDate(ShowcaseApp* app) {
    if (app->calYear > 0 && app->calMonth > 0) {
        return;
    }
    SYSTEMTIME st = {};
    GetLocalTime(&st);
    app->calYear = st.wYear;
    app->calMonth = st.wMonth;
}

El* ShowcaseCalendarGrid(ShowcaseApp* app, Arena* a) {
    EnsureCalendarDate(app);
    SYSTEMTIME today = {};
    GetLocalTime(&today);
    int y = app->calYear;
    int m = app->calMonth;
    El* root = Calendar::New(a, StrL("example-calendar"))->FlexCol()->W(250)->Pad(12)->Border(1, Rgb(0xd4, 0xd4, 0xd4));

    El* nav = Div(a)->FlexRow()->W(kFill)->ItemsCenter()->JustifyBetween();
    nav->Child(Div(a)
                   ->W(28)
                   ->H(28)
                   ->ItemsCenter()
                   ->JustifyCenter()
                   ->HoverBg(ScHover())
                   ->Click(ClickCalPrev)
                   ->FocusId(ClickCalPrev)
                   ->Child(ScTxt(a, StrL("‹"), 14, ScInk())));
    El* title = Div(a)->FlexRow()->Gap(16)->ItemsCenter();
    title->Child(ScTxt(a, Str(kMon[m]), 12, ScInk()));
    title->Child(ScTxt(a, DupFmt(a, "%d", y), 12, ScInk()));
    nav->Child(title);
    nav->Child(Div(a)
                   ->W(28)
                   ->H(28)
                   ->ItemsCenter()
                   ->JustifyCenter()
                   ->HoverBg(ScHover())
                   ->Click(ClickCalNext)
                   ->FocusId(ClickCalNext)
                   ->Child(ScTxt(a, StrL("›"), 14, ScInk())));
    root->Child(nav);

    El* wd = Div(a)->FlexRow();
    for (int i = 0; i < 7; i++) {
        wd->Child(Div(a)->W(32)->H(32)->ItemsCenter()->JustifyCenter()->Child(ScTxt(a, Str(kWd[i]), 12, ScMutedC())));
    }
    root->Child(wd);

    int first = Dow(y, m, 1); // 0=Sun
    int dim = DaysInMonth(y, m);
    int prevM = m == 1 ? 12 : m - 1;
    int prevY = m == 1 ? y - 1 : y;
    int prevDim = DaysInMonth(prevY, prevM);
    int cell = 0;
    for (int r = 0; r < 6; r++) {
        El* row = Div(a)->FlexRow();
        for (int c = 0; c < 7; c++, cell++) {
            int day;
            bool muted = false;
            if (cell < first) {
                day = prevDim - first + cell + 1;
                muted = true;
            } else if (cell - first + 1 > dim) {
                day = cell - first + 1 - dim;
                muted = true;
            } else {
                day = cell - first + 1;
            }
            bool active = !muted && app->calDay > 0 && day == app->calDay && m == app->calMonth && y == app->calYear;
            bool isToday = !muted && day == (int)today.wDay && m == (int)today.wMonth && y == (int)today.wYear;
            El* d = Div(a)
                        ->W(32)
                        ->H(32)
                        ->ItemsCenter()
                        ->JustifyCenter()
                        ->Click(ClickCalDay + cell)
                        ->FocusId(ClickCalDay + cell);
            if (active) {
                d->Bg(ScInk())->Child(ScTxt(a, DupFmt(a, "%d", day), 12, ScWhite()));
            } else {
                if (isToday) {
                    d->Border(1, ScBorder());
                }
                d->HoverBg(ScHover())->Child(ScTxt(a, DupFmt(a, "%d", day), 12, muted ? ScSilver() : ScInk()));
            }
            row->Child(d);
        }
        root->Child(row);
    }
    return root;
}

El* ShowcaseCalendar(ShowcaseApp* app, Arena* a) {
    return ShowcaseCalendarGrid(app, a);
}

void ShowcaseCalendarClick(ShowcaseApp* app, int id) {
    if (id == ClickCalPrev) {
        app->calMonth--;
        if (app->calMonth < 1) {
            app->calMonth = 12;
            app->calYear--;
        }
    } else if (id == ClickCalNext) {
        app->calMonth++;
        if (app->calMonth > 12) {
            app->calMonth = 1;
            app->calYear++;
        }
    } else if (id >= ClickCalDay && id < ClickCalDay + 42) {
        int cell = id - ClickCalDay;
        int first = Dow(app->calYear, app->calMonth, 1);
        int dim = DaysInMonth(app->calYear, app->calMonth);
        int day = cell - first + 1;
        if (day >= 1 && day <= dim) {
            app->calDay = day;
        } else if (day < 1) {
            ShowcaseCalendarClick(app, ClickCalPrev);
            app->calDay = DaysInMonth(app->calYear, app->calMonth) + day;
        } else {
            int over = day - dim;
            ShowcaseCalendarClick(app, ClickCalNext);
            app->calDay = over;
        }
    }
}

SHOWCASE_PAGE(CompCalendar, ShowcaseCalendar, ShowcaseCalendarClick);

