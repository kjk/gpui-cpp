#include "gpui.h"

using namespace gpui;

static const int kMaxHist = 120;
static const int kKeepProcs = 200;

enum ClickId : int {
    ClickTabSystem = 1,
    ClickTabProcesses = 2,
    ClickSortPid = 10,
    ClickSortName = 11,
    ClickSortCpu = 12,
    ClickSortMem = 13,
    ClickMin = ClickWinMin,
    ClickMax = ClickWinMax,
    ClickClose = ClickWinClose,
    ClickDrag = ClickWinCaption,
};

struct MonitorApp {
    static El* Render(MonitorApp* self, Ctx* cx);
    // Dropped with the entity; this is what onShutdown used to do.
    ~MonitorApp() { SysStateFree(&sys); }

    SysState sys;
    float cpuHist[kMaxHist] = {};
    float memHist[kMaxHist] = {};
    int histN = 0;
    int timeIndex = 0;
    int tab = 0;
    ProcessSort sort = ProcessSort::Cpu;
    bool sortDesc = true;
    float tableScroll = 0;
};

static void PushHist(MonitorApp* app, float cpu, float mem) {
    if (app->histN < kMaxHist) {
        app->cpuHist[app->histN] = cpu;
        app->memHist[app->histN] = mem;
        app->histN++;
    } else {
        memmove(app->cpuHist, app->cpuHist + 1, sizeof(float) * (kMaxHist - 1));
        memmove(app->memHist, app->memHist + 1, sizeof(float) * (kMaxHist - 1));
        app->cpuHist[kMaxHist - 1] = cpu;
        app->memHist[kMaxHist - 1] = mem;
    }
    app->timeIndex++;
}

static void Collect(MonitorApp* app) {
    SysRefresh(&app->sys);
    SysSortProcesses(&app->sys, app->sort, app->sortDesc, kKeepProcs);
    PushHist(app, app->sys.cpu, app->sys.mem);
}

static void OnTick(MonitorApp* app, Ctx* cx, const TickEvent*) {
    Window* host = cx->win;

    Collect(app);
}

static void OnClick(MonitorApp* app, Ctx* cx, const ClickEvent* ev) {
    Window* host = cx->win;
    int id = ev->id;
    if (id == ClickTabSystem) {
        app->tab = 0;
        return;
    }
    if (id == ClickTabProcesses) {
        app->tab = 1;
        return;
    }
    if (id == ClickMin) {
        AppMinimize(host);
        return;
    }
    if (id == ClickMax) {
        AppToggleMaximize(host);
        return;
    }
    if (id == ClickClose) {
        AppClose(host);
        return;
    }
    if (id == ClickDrag) {
        AppDrag(host);
        return;
    }
    ProcessSort field = ProcessSort::Cpu;
    bool isSort = true;
    if (id == ClickSortPid) {
        field = ProcessSort::Pid;
    } else if (id == ClickSortName) {
        field = ProcessSort::Name;
    } else if (id == ClickSortCpu) {
        field = ProcessSort::Cpu;
    } else if (id == ClickSortMem) {
        field = ProcessSort::Memory;
    } else {
        isSort = false;
    }
    if (isSort) {
        if (app->sort == field) {
            app->sortDesc = !app->sortDesc;
        } else {
            app->sort = field;
            app->sortDesc =
                field != ProcessSort::Name && field != ProcessSort::Pid;
        }
        SysSortProcesses(&app->sys, app->sort, app->sortDesc, kKeepProcs);
    }
}

static void OnWheel(MonitorApp* app, Ctx* cx, const WheelEvent* ev) {
    (void)cx;
    float x = ev->x;
    float y = ev->y;
    float delta = ev->delta;
    (void)x;
    if (app->tab != 1) {
        return;
    }
    if (y < 34) {
        return;
    }
    app->tableScroll -= delta;
    if (app->tableScroll < 0) {
        app->tableScroll = 0;
    }
    float maxScroll = (float)app->sys.procs.len * 28.f;
    if (app->tableScroll > maxScroll) {
        app->tableScroll = maxScroll;
    }
}

static El* SegmentedTab(Arena* a, Str label, bool selected, int id) {
    const Theme& th = ThemeDark();
    El* t = Div(a)
                ->H(24)
                ->PadX(12)
                ->ItemsCenter()
                ->JustifyCenter()
                ->Radius(6)
                ->Click(id)
                ->Child(TextEl(a, label)->Font(13)->Fg(selected ? th.tabActiveFg
                                                                : th.tabFg));
    if (selected) {
        t->Bg(th.tabActiveBg);
    }
    return t;
}

static El* TitleBar(Arena* a, Window* host, MonitorApp* app) {
    const Theme& th = ThemeDark();
    Rgba mixed = RgbaMix(th.titleBar, th.background, 0.55f);

    El* tabs = Div(a)
                   ->FlexRow()
                   ->ItemsCenter()
                   ->Gap(2)
                   ->Pad(2)
                   ->Radius(8)
                   ->Bg(th.tabBar)
                   ->Child(SegmentedTab(a, StrL("System"), app->tab == 0,
                                        ClickTabSystem))
                   ->Child(SegmentedTab(a, StrL("Processes"), app->tab == 1,
                                        ClickTabProcesses));

    double gb = (double)app->sys.memTotal / (1024.0 * 1024.0 * 1024.0);
    El* memLabel = TextEl(a, fmt("%.1f GB", gb))->Font(12)->Fg(th.mutedFg);

    El* spacer = Div(a)->Grow()->H(34);

    El* bar = Div(a)
                  ->FlexRow()
                  ->H(34)
                  ->PadL(12)
                  ->PadR(12)
                  ->ItemsCenter()
                  ->Bg(mixed)
                  ->Child(tabs)
                  ->Child(spacer)
                  ->Child(memLabel);

    // bottom border under title bar
    El* wrap = Div(a)->FlexCol()->Shrink0()->W(kFill)->Child(bar)->Child(
        Div(a)->H(1)->W(kFill)->Bg(th.titleBarBorder));
    return wrap;
}

static El* ChartCard(Arena* a, Str title, const float* ys, int n, float current,
                     Rgba color) {
    const Theme& th = ThemeDark();
    El* header =
        Div(a)
            ->FlexRow()
            ->JustifyBetween()
            ->ItemsCenter()
            ->Shrink0()
            ->PadX(12)
            ->PadY(4)
            ->Child(TextEl(a, title)->Font(14)->Fg(th.foreground))
            ->Child(TextEl(a, FormatPct(current, 1))->Font(14)->Fg(color));

    Rgba fillTop = RgbaOpacity(color, 0.4f);
    Rgba fillBot = RgbaOpacity(th.background, 0.1f);
    El* chart = ChartEl(a, ys, n, color, fillTop, fillBot, 15);

    return Div(a)
        ->FlexCol()
        ->Grow()
        ->MinH(160)
        ->Gap(8)
        ->Border(1, th.border)
        ->Child(header)
        ->Child(chart);
}

static El* SystemTab(Arena* a, MonitorApp* app) {
    const Theme& th = ThemeDark();
    float cpu = app->histN ? app->cpuHist[app->histN - 1] : 0;
    float mem = app->histN ? app->memHist[app->histN - 1] : 0;
    return Div(a)
        ->FlexCol()
        ->Grow()
        ->Pad(12)
        ->Gap(16)
        ->Child(ChartCard(a, StrL("CPU Usage"), app->cpuHist, app->histN, cpu,
                          th.red))
        ->Child(ChartCard(a, StrL("Memory Usage"), app->memHist, app->histN,
                          mem, th.blue));
}

static const float kColW[4] = {70, 380, 80, 100};
static const int kSortClick[4] = {ClickSortPid, ClickSortName, ClickSortCpu,
                                  ClickSortMem};

static Str SortMark(ProcessSort field, ProcessSort cur, bool desc) {
    if (field != cur) {
        return StrL("");
    }
    return desc ? StrL(" ↓") : StrL(" ↑");
}

static El* ProcTableHeader(Arena* a, MonitorApp* app) {
    const Theme& th = ThemeDark();
    const char* names[4] = {"PID", "Name", "CPU %", "Memory"};
    ProcessSort fields[4] = {ProcessSort::Pid, ProcessSort::Name,
                             ProcessSort::Cpu, ProcessSort::Memory};
    El* row = Div(a)->FlexRow()->H(28)->Shrink0()->ItemsCenter()->Bg(
        Rgba8(0x17, 0x17, 0x17, 0x66));
    for (int i = 0; i < 4; i++) {
        TempStr lab = fmt("%s%s", Str(names[i]),
                          SortMark(fields[i], app->sort, app->sortDesc));
        row->Child(Div(a)
                       ->W(kColW[i])
                       ->H(28)
                       ->PadX(8)
                       ->ItemsCenter()
                       ->Click(kSortClick[i])
                       ->Child(TextEl(a, lab)->Font(12)->Fg(th.tableHeadFg)));
    }
    return row;
}

static Rgba CpuColor(const Theme& th, float cpu) {
    if (cpu > 50) {
        return th.red;
    }
    if (cpu > 20) {
        return th.yellow;
    }
    return th.blue;
}

static El* ProcTableRow(Arena* a, const ProcessInfo* p, int ix) {
    const Theme& th = ThemeDark();
    El* row = Div(a)->FlexRow()->H(28)->Shrink0()->ItemsCenter();
    if (ix % 2 == 1) {
        row->Bg(th.tableEven);
    }
    row->Child(Div(a)->W(kColW[0])->H(28)->PadX(8)->ItemsCenter()->Child(
        TextEl(a, fmt("%d", (int)p->pid))->Font(12)->Fg(th.mutedFg)));
    row->Child(Div(a)->W(kColW[1])->H(28)->PadX(8)->ItemsCenter()->Child(
        TextEl(a, Str(p->name))
            ->Font(14)
            ->Fg(th.foreground)
            ->Truncate()
            ->W(kColW[1] - 16)));
    row->Child(Div(a)->W(kColW[2])->H(28)->PadX(8)->ItemsCenter()->Child(
        TextEl(a, FormatPct(p->cpu, 1))->Font(12)->Fg(CpuColor(th, p->cpu))));
    row->Child(Div(a)->W(kColW[3])->H(28)->PadX(8)->ItemsCenter()->Child(
        TextEl(a, FormatBytes(p->memory))->Font(12)->Fg(th.green)));
    return row;
}

static El* ProcessesTab(Arena* a, MonitorApp* app) {
    El* body = Div(a)->FlexCol()->Grow()->ClipY();
    int n = app->sys.procs.len;
    // virtualize a bit: skip rows above scroll
    int first = (int)(app->tableScroll / 28.f);
    if (first < 0) {
        first = 0;
    }
    if (first > 0) {
        body->Child(Div(a)->H((float)first * 28.f)->Shrink0());
    }
    int last = first + 40;
    if (last > n) {
        last = n;
    }
    for (int i = first; i < last; i++) {
        body->Child(ProcTableRow(a, &app->sys.procs[i], i));
    }
    if (last < n) {
        body->Child(Div(a)->H((float)(n - last) * 28.f)->Shrink0());
    }

    return Div(a)
        ->FlexCol()
        ->SizeFull()
        ->Child(ProcTableHeader(a, app))
        ->Child(body);
}

static El* StatusChip(Arena* a, IconName icon, float pct) {
    const Theme& th = ThemeDark();
    return Div(a)
        ->FlexRow()
        ->W(135)
        ->Gap(8)
        ->ItemsCenter()
        ->Child(IconEl(a, icon)->Fg(th.mutedFg))
        ->Child(ProgressEl(a, pct, 48, 8))
        ->Child(TextEl(a, FormatPct(pct, 0))->Font(14)->Fg(th.mutedFg));
}

static El* StatusBar(Arena* a, MonitorApp* app) {
    const Theme& th = ThemeDark();
    float cpu = app->histN ? app->cpuHist[app->histN - 1] : 0;
    float mem = app->histN ? app->memHist[app->histN - 1] : 0;

    El* left =
        Div(a)
            ->FlexRow()
            ->Gap(16)
            ->ItemsCenter()
            ->Child(StatusChip(a, IconName::HardDrive, app->sys.disk.usedPct))
            ->Child(StatusChip(a, IconName::MemoryStick, mem))
            ->Child(StatusChip(a, IconName::Cpu, cpu));

    El* right = Div(a);
    if (app->sys.battery.present) {
        IconName bi = IconName::Battery;
        if (app->sys.battery.charging) {
            bi = IconName::BatteryCharging;
        } else if (app->sys.battery.pct >= 80) {
            bi = IconName::BatteryFull;
        } else if (app->sys.battery.pct >= 30) {
            bi = IconName::BatteryMedium;
        }
        right->FlexRow()
            ->Gap(8)
            ->ItemsCenter()
            ->Child(IconEl(a, bi)->Fg(th.mutedFg))
            ->Child(TextEl(a, FormatPct(app->sys.battery.pct, 0))
                        ->Font(14)
                        ->Fg(th.mutedFg));
    }

    return Div(a)
        ->FlexRow()
        ->H(28)
        ->PadX(12)
        ->ItemsCenter()
        ->JustifyBetween()
        ->BorderT(1, th.border)
        ->Bg(th.tabBar)
        ->Child(left)
        ->Child(right);
}

El* MonitorApp::Render(MonitorApp* app, Ctx* cx) {
    Arena* frame = cx->a;
    Window* host = cx->win;

    WinSize size = WindowSize(cx->win);

    const Theme& th = ThemeDark();

    El* content = Div(frame)->FlexCol()->Grow()->ClipY();
    if (app->tab == 0) {
        content->Child(SystemTab(frame, app));
    } else {
        content->Child(ProcessesTab(frame, app));
    }

    return Div(frame)
        ->FlexCol()
        ->SizeFull()
        ->Bg(th.background)
        ->Child(TitleBar(frame, host, app))
        ->Child(content)
        ->Child(StatusBar(frame, app));
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    App* app = AppNew();
    Entity<MonitorApp> view = EntityNew<MonitorApp>(app);
    MonitorApp* self = view.Get(app);
    (void)self;
    ThemeSet(ThemeMode::Dark);
    SysStateInit(&self->sys);
    Collect(self);
    AppWinOpts opts = {};
    Window* win =
        WindowOpenView(app, L"System Monitor", 680, 600, view.id, opts);
    WindowOnClick(win, ListenTo(view, &OnClick));
    WindowOnWheel(win, ListenTo(view, &OnWheel));
    WindowSetInterval(win, 500, ListenTo(view, &OnTick));
    int rc = AppRun(app);
    AppFree(app);
    return rc;
}
