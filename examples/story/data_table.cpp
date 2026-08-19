#include "Story.h"

// The Rust story fills the table with random stocks; ours keeps a fixed set
// with the same columns.
struct Stock {
    const char* market;
    const char* name;
    const char* symbol;
    const char* price;
    const char* chg;
    const char* pct;
    bool up;
};

static const Stock kStocks[] = {
    {"US", "Coinbase Global Inc.", "COIN.US", "536.27", "+37.79", "+7.05%",
     true},
    {"US", "Lowe's Companies Inc.", "LOW.US", "404.59", "-22.41", "-5.54%",
     false},
    {"US", "Zoetis Inc.", "ZTS.US", "633.99", "-62.24", "-9.82%", false},
    {"US", "Tesla Inc.", "TSLA.US", "949.92", "+30.37", "+3.20%", true},
    {"HK", "Xiaomi Corp.", "1810.HK", "462.56", "-1.66", "-0.36%", false},
    {"US", "PepsiCo Inc.", "PEP.US", "75.89", "+1.39", "+1.83%", true},
    {"US", "Viomi Technology Co. Lt...", "VIOT.US", "927.28", "-9.54", "-1.03%",
     false},
    {"HK", "CSOP FTSE China A50 ETF", "2822.HK", "632.39", "+47.13", "+7.45%",
     true},
    {"US", "Zepp Health Corp. ADR", "ZEPP.US", "401.68", "+38.18", "+9.51%",
     true},
    {"HK", "China Oilfield Services Ltd.", "2883.HK", "854.84", "+82.09",
     "+9.60%", true},
    {"US", "Zscaler Inc.", "ZS.US", "145.76", "-2.00", "-1.37%", false},
    {"US", "American Express Co.", "AXP.US", "484.57", "+36.92", "+7.62%",
     true},
    {"US", "Workday Inc.", "WDAY.US", "111.25", "-1.46", "-1.31%", false},
    {"US", "SOS Ltd. ADR", "SOS.US", "712.91", "+52.24", "+7.33%", true},
    {"US", "Unity Software Inc.", "U.US", "609.16", "+49.35", "+8.10%", true},
    {"US", "Uber Technologies Inc.", "UBER.US", "198.21", "+17.81", "+8.99%",
     true},
    {"HK", "China Resources Power...", "0836.HK", "300.31", "-20.94", "-6.97%",
     false},
};

enum {
    DtMenuSize = 1,
    DtMenuRows,
    DtMenuExtra,
    DtMenuOptions,
    DtMenuGoTo
};

enum {
    DtActRows = 500,   // + index into kRowCounts
    DtActExtra = 520,  // + index into kExtraCounts
    DtActOption = 540, // + index into kDtOptions
    DtActGoTo = 560
};

static const int kRowCounts[] = {100, 1000, 5000, 100000};
static const int kExtraCounts[] = {0, 5, 20, 50};
static const char* kDtOptions[] = {"Loop selection", "Col resize",
                                   "Col order",      "Col sort",
                                   "Stripe",         "Refresh data"};
static const char* kGoToRows[] = {"Top", "Selected", "Row 50", "Bottom"};

// The order the rows are shown in, which is what the delegate's perform_sort
// rewrites. -1 means the table's own order.
struct DataTableStory {
    int rowCount = 2; // 5000
    int extra = 0;    // 0 extra columns
    int openMenu = 0;
    bool options[6] = {false, true, true, true, false, false};
    StoryToolbarState toolbar;
    // TableState is an entity in Rust too, which is what the row and head
    // closures capture.
    Entity<TableState> table = {};
    bool seeded = false;
    // What the last table event said, shown under the table.
    Str message = {};
    // The order the rows are shown in, which is what the delegate's
    // perform_sort rewrites.
    int order[17] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

    ~DataTableStory() { StrFree(message); }
    static El* Render(DataTableStory* self, Ctx* cx);
    static void OnKey(DataTableStory* self, Ctx* cx, const KeyEvent* ev);
};

// The delegate's perform_sort: the story reorders its own rows, which is the
// whole point of the event.
static float StockKey(const Stock& s, int col) {
    switch (col) {
        case 4:
            return (float)atof(s.price);
        case 5:
            return (float)atof(s.chg);
        default:
            return (float)atof(s.pct);
    }
}

static void SortRows(DataTableStory* self, int col, ColumnSort dir) {
    const int n = (int)(sizeof(kStocks) / sizeof(kStocks[0]));
    for (int i = 0; i < n; i++) {
        self->order[i] = i;
    }
    if (dir == ColumnSort::Default) {
        return;
    }
    bool asc = dir == ColumnSort::Ascending;
    // An insertion sort: seventeen rows, and it keeps equal rows in the order
    // the table had them.
    for (int i = 1; i < n; i++) {
        int v = self->order[i];
        int j = i - 1;
        while (j >= 0) {
            bool swap;
            if (col == 3) {
                int c =
                    strcmp(kStocks[self->order[j]].symbol, kStocks[v].symbol);
                swap = asc ? c > 0 : c < 0;
            } else {
                float a = StockKey(kStocks[self->order[j]], col);
                float b = StockKey(kStocks[v], col);
                swap = asc ? a > b : a < b;
            }
            if (!swap) {
                break;
            }
            self->order[j + 1] = self->order[j];
            j--;
        }
        self->order[j + 1] = v;
    }
}

// cx.subscribe(&table, ..): the story says what it was told.
static void OnTableEvent(DataTableStory* self, Ctx* cx, const TableEvent* ev) {
    StrFree(self->message);
    switch (ev->kind) {
        case TableEventKind::SelectRow:
            self->message = StrDup(fmt("Selected row %d", ev->row));
            break;
        case TableEventKind::SelectCol:
            self->message = StrDup(fmt("Selected column %d", ev->col));
            break;
        case TableEventKind::DoubleClickedRow:
            self->message = StrDup(fmt("Double clicked row %d", ev->row));
            break;
        case TableEventKind::Sort:
            SortRows(self, ev->col, ev->sort);
            self->message = StrDup(
                fmt("Sorted column %d %s", ev->col,
                    Str(ev->sort == ColumnSort::Ascending    ? "ascending"
                        : ev->sort == ColumnSort::Descending ? "descending"
                                                             : "off")));
            break;
        default:
            self->message = StrDup(StrL("Selection cleared"));
            break;
    }
    Notify(cx);
}

// The table's key context: the arrows walk the selection, Home and End take
// the first and last column, the page keys move a page, Escape gives up.
void DataTableStory::OnKey(DataTableStory* self, Ctx* cx, const KeyEvent* ev) {
    if (!ev->down) {
        return;
    }
    TableAction act = TableActionForKey(ev->vk);
    if (act == TableAction::None) {
        return;
    }
    TableState* st = self->table.Get(cx);
    if (st) {
        TablePerform(st, cx, act);
    }
}

static void DtMenuOpen(DataTableStory* self, Ctx* cx, const ClickEvent*,
                       intptr_t which) {
    self->openMenu = self->openMenu == (int)which ? 0 : (int)which;
    Notify(cx);
}
static void DtMenuAct(DataTableStory* self, Ctx* cx, const ClickEvent*,
                      intptr_t act) {
    if (act >= DtActGoTo) {
        // Scrolling is not wired up here.
    } else if (act >= DtActOption) {
        int i = (int)(act - DtActOption);
        self->options[i] = !self->options[i];
    } else if (act >= DtActExtra) {
        self->extra = (int)(act - DtActExtra);
    } else if (act >= DtActRows) {
        self->rowCount = (int)(act - DtActRows);
    }
    self->openMenu = 0;
    Notify(cx);
}

// render_td: the delegate's cell, which the table places and styles.
static El* DtCellFor(Ctx* cx, void* data, int row, int col) {
    DataTableStory* self = (DataTableStory*)data;
    const Theme& th = cx->theme();
    const Stock& s = kStocks[self->order[row]];
    Rgba trend = s.up ? th.green : th.red;
    switch (col) {
        case 0:
            return StoryTxt(cx, StoryFmt(cx, "%d", self->order[row]), 16,
                            th.mutedFg)
                ->LineHeight(1.f);
        case 1:
            return StoryTxt(cx, Str(s.market), 16, th.blue)->LineHeight(1.f);
        case 2:
            // The Rust cell hides its overflow; ours truncates the text.
            return StoryTxt(cx, Str(s.name), 16, th.foreground)
                ->LineHeight(1.f)
                ->MaxW(160)
                ->Truncate();
        case 3:
            return StoryTxt(cx, Str(s.symbol), 16, th.foreground)
                ->Medium()
                ->LineHeight(1.f);
        case 4:
            return StoryTxt(cx, Str(s.price), 16, th.foreground)
                ->LineHeight(1.f);
        case 5:
            return StoryTxt(cx, Str(s.chg), 16, trend)->LineHeight(1.f);
        default:
            return StoryTxt(cx, Str(s.pct), 16, trend)->LineHeight(1.f);
    }
}

El* DataTableStory::Render(DataTableStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    Listener openMenu = Listen(cx, &DtMenuOpen);
    Listener act = Listen(cx, &DtMenuAct);
    if (!self->seeded) {
        self->seeded = true;
        self->table = EntityNewState<TableState>(cx->app);
    }
    El* page = Div(a)->FlexCol()->Gap(16)->W(kFill);

    // One group holding the size, rows, extra columns, options, go-to and
    // export controls.
    El* toolbarRow = Div(a)->FlexRow()->W(kFill)->JustifyEnd()->ItemsStart();
    El* group = StoryToolbarGroup(cx);
    group->Child(StoryToolbarDropdown(cx, StrL("dt-size"), StrL("Size: Medium"),
                                      false, ListenerArg(openMenu, DtMenuSize),
                                      nullptr, 0, act));
    group->Child(StoryToolbarDivider(cx));
    StoryToolbarOpt rowRows[4];
    for (int i = 0; i < 4; i++) {
        rowRows[i].label = i == 0   ? "100"
                           : i == 1 ? "1000"
                           : i == 2 ? "5000"
                                    : "100000";
        rowRows[i].checked = self->rowCount == i;
        rowRows[i].act = DtActRows + i;
    }
    group->Child(StoryToolbarDropdown(
        cx, StrL("dt-rows"),
        StoryFmt(cx, "Rows: %d", kRowCounts[self->rowCount]),
        self->openMenu == DtMenuRows, ListenerArg(openMenu, DtMenuRows),
        rowRows, 4, act));
    group->Child(StoryToolbarDivider(cx));
    StoryToolbarOpt extraRows[4];
    for (int i = 0; i < 4; i++) {
        extraRows[i].label = i == 0 ? "0" : i == 1 ? "5" : i == 2 ? "20" : "50";
        extraRows[i].checked = self->extra == i;
        extraRows[i].act = DtActExtra + i;
    }
    group->Child(StoryToolbarDropdown(
        cx, StrL("dt-extra"),
        StoryFmt(cx, "Extra Columns: %d", kExtraCounts[self->extra]),
        self->openMenu == DtMenuExtra, ListenerArg(openMenu, DtMenuExtra),
        extraRows, 4, act));
    group->Child(StoryToolbarDivider(cx));
    StoryToolbarOpt optRows[6];
    for (int i = 0; i < 6; i++) {
        optRows[i].label = kDtOptions[i];
        optRows[i].checked = self->options[i];
        optRows[i].act = DtActOption + i;
    }
    group->Child(StoryToolbarDropdown(cx, StrL("dt-options"), StrL("Options"),
                                      self->openMenu == DtMenuOptions,
                                      ListenerArg(openMenu, DtMenuOptions),
                                      optRows, 6, act));
    group->Child(StoryToolbarDivider(cx));
    StoryToolbarOpt goRows[4];
    for (int i = 0; i < 4; i++) {
        goRows[i].label = kGoToRows[i];
        goRows[i].act = DtActGoTo + i;
        goRows[i].plain = true;
    }
    group->Child(StoryToolbarDropdown(
        cx, StrL("dt-go-to"), StrL("Go To"), self->openMenu == DtMenuGoTo,
        ListenerArg(openMenu, DtMenuGoTo), goRows, 4, act));
    group->Child(StoryToolbarDivider(cx));
    El* exportBtn =
        Div(a)
            ->H(24)
            ->PadX(8)
            ->ItemsCenter()
            ->JustifyCenter()
            ->HoverBg(th.muted)
            ->Child(StoryTxt(cx, StrL("Export CSV"), 14, th.foreground));
    group->Child(exportBtn);
    toolbarRow->Child(group);
    page->Child(toolbarRow);

    // Column widths, in the order the Rust delegate declares them.
    const float wId = 45, wMarket = 61, wName = 176, wSymbol = 101,
                wPrice = 100, wChg = 100, wPct = 110;

    // Two levels of grouped headers over the column row.
    El* group1 = Div(a)->FlexRow()->W(kFill)->BorderB(1, th.border);
    group1->Child(Div(a)
                      ->FlexRow()
                      ->W(wId + wMarket + wName + wSymbol)
                      ->PadY(6)
                      ->JustifyCenter()
                      ->BorderR(1, th.border)
                      ->Child(StoryTxt(cx, StrL("Stock"), 16, th.foreground)
                                  ->LineHeight(1.f)));
    group1->Child(Div(a)->Grow()->PadY(6));
    El* group2 = Div(a)->FlexRow()->W(kFill)->BorderB(1, th.border);
    group2->Child(Div(a)
                      ->FlexRow()
                      ->W(wId + wMarket + wName + wSymbol)
                      ->PadY(6)
                      ->JustifyCenter()
                      ->BorderR(1, th.border)
                      ->Child(StoryTxt(cx, StrL("Identity"), 16, th.foreground)
                                  ->LineHeight(1.f)));
    group2->Child(Div(a)->Grow()->FlexRow()->PadY(6)->JustifyCenter()->Child(
        StoryTxt(cx, StrL("Price & Change"), 16, th.foreground)
            ->LineHeight(1.f)));

    static const component::TableColumn kColumns[] = {
        {StrL("ID"), wId, false, false, false},
        {StrL("Market"), wMarket, false, false, true},
        {StrL("Name"), wName, false, false, true},
        {StrL("Symbol"), wSymbol, false, true, true},
        {StrL("Price"), wPrice, true, true, true},
        {StrL("Chg"), wChg, true, true, true},
        {StrL("Chg%"), wPct, true, true, true},
    };
    const int nColumns = (int)(sizeof(kColumns) / sizeof(kColumns[0]));
    const int nStocks = (int)(sizeof(kStocks) / sizeof(kStocks[0]));
    TableState* st = self->table.Get(cx);
    if (st) {
        st->sortable = self->options[3];
        st->loopSelection = self->options[0];
        st->onEvent = Listen(cx, &OnTableEvent);
    }
    El* box = component::DataTable::New(cx, StrL("data-table"), self->table)
                  ->Columns(kColumns, nColumns)
                  ->Rows(nStocks, self, DtCellFor)
                  ->Stripe(self->options[4])
                  ->GroupHeader(group1)
                  ->GroupHeader(group2)
                  ->IntoEl();

    // The status line under the table.
    El* status = Div(a)
                     ->FlexRow()
                     ->W(kFill)
                     ->PadX(12)
                     ->PadY(8)
                     ->JustifyBetween()
                     ->ItemsCenter();
    status->Child(StoryTxt(
        cx,
        StoryFmt(cx, "Total · %d rows · %d columns", kRowCounts[self->rowCount],
                 45 + kExtraCounts[self->extra]),
        14, th.mutedFg));
    status->Child(StoryTxt(cx, StrL("Current · rows 0..21 · columns 0..10"), 14,
                           th.mutedFg));
    page->Child(box);
    page->Child(status);
    if (self->message.s) {
        page->Child(StoryTxt(cx, self->message, 14, th.mutedFg));
    }
    return page;
}

STORY_PAGE_KEYS(StoryDataTable, DataTableStory);
