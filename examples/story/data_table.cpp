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
    DtActSize = 480,   // + index into kSizes
    DtActRows = 500,   // + index into kRowCounts
    DtActExtra = 520,  // + index into kExtraCounts
    DtActOption = 540, // + index into kDtOptions
    DtActClear = 559,
    DtActGoTo = 560
};

// The rows and columns the toolbar can ask for, and how each reads.
static const int kRowCounts[] = {100, 500, 5000, 10000, 1000000};
static const char* const kRowLabels[] = {"100", "500", "5,000", "10,000",
                                         "1,000,000"};
static const int kNRowCounts = 5;
static const int kExtraCounts[] = {0, 4, 8, 16, 32};
static const char* const kExtraLabels[] = {"None", "4", "8", "16", "32"};
static const int kNExtraCounts = 5;
// Size::table_row_height: 48px, Large, Medium, Small, XSmall.
static const float kSizeRowH[] = {48, 40, 32, 30, 26};
static const char* const kSizeLabels[] = {"48px", "Large", "Medium", "Small",
                                          "XSmall"};
static const int kNSizes = 5;

// The Options dropdown, in Rust's own order. Fixed Column is the one row not
// here: the table has no frozen column to turn on.
enum {
    DtOptLoop = 0,
    DtOptColResize,
    DtOptColOrder,
    DtOptSortable,
    DtOptColSelect,
    DtOptRowSelect,
    DtOptCellSelect,
    DtOptRowHeader,
    DtOptStriped,
    DtOptLoading,
    DtOptLazyLoad,
    DtOptRefresh,
    DtOptGroupHeaders,
    DtOptCount
};
static const char* const kDtOptions[DtOptCount] = {
    "Loop Selection",    "Column Resize",  "Column Order",    "Sortable",
    "Column Selectable", "Row Selectable", "Cell Selectable", "Row Header",
    "Striped Rows",      "Loading",        "Lazy Load",       "Refresh Data",
    "Group Headers"};
static const char* const kGoToRows[] = {"Top", "Bottom", "Cell 5:3",
                                        "Cell 10:7"};
static const int kNGoTo = 4;

// The order the rows are shown in, which is what the delegate's perform_sort
// rewrites. -1 means the table's own order.
struct DataTableStory {
    int rowCount = 2; // 5,000
    int extra = 0;    // no extra columns
    int size = 2;     // Medium
    int openMenu = 0;
    bool options[DtOptCount] = {false, true,  true,  true,  true,  true, false,
                                false, false, false, false, false, true};
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
        case TableEventKind::MoveColumn:
            self->message =
                StrDup(fmt("Moved column %d to %d", ev->col, ev->row));
            break;
        case TableEventKind::ColumnWidthsChanged: {
            // ColumnWidthsChanged carries every column's width, not just the
            // one the drag moved.
            StrBuilder sb;
            sb.Append(StrL("Column widths"));
            for (int i = 0; i < ev->nWidths; i++) {
                sb.Append(fmt(" %d", (int)ev->widths[i]));
            }
            self->message = sb.TakeStr();
            break;
        }
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
        // Top and Bottom scroll_to_row; the other two set_selected_cell.
        TableState* st = self->table.Get(cx);
        int which = (int)(act - DtActGoTo);
        if (st && which == 0) {
            TableScrollToRow(st, 0, ScrollStrategy::Center);
        } else if (st && which == 1) {
            TableScrollToRow(st, st->rowCount - 1, ScrollStrategy::Center);
        } else if (st && which == 2) {
            TableSetSelectedCell(st, cx, 5, 3);
        } else if (st) {
            TableSetSelectedCell(st, cx, 10, 7);
        }
    } else if (act == DtActClear) {
        TableState* st = self->table.Get(cx);
        if (st) {
            TableClearSelection(st, cx);
        }
    } else if (act >= DtActOption) {
        int i = (int)(act - DtActOption);
        self->options[i] = !self->options[i];
    } else if (act >= DtActExtra) {
        self->extra = (int)(act - DtActExtra);
    } else if (act >= DtActRows) {
        self->rowCount = (int)(act - DtActRows);
    } else if (act >= DtActSize) {
        self->size = (int)(act - DtActSize);
    }
    self->openMenu = 0;
    Notify(cx);
}

// render_td: the delegate's cell, which the table places and styles.
static El* DtCellFor(Ctx* cx, void* data, int row, int col) {
    DataTableStory* self = (DataTableStory*)data;
    const Theme& th = cx->theme();
    // The Rust story generates a row per index; ours repeats the fixed set,
    // so a table of five thousand rows is five thousand rows to scroll.
    const int nStocks = (int)(sizeof(kStocks) / sizeof(kStocks[0]));
    const Stock& s = kStocks[self->order[row % nStocks]];
    Rgba trend = s.up ? th.green : th.red;
    switch (col) {
        case 0:
            return StoryTxt(cx, StoryFmt(cx, "%d", row), 16, th.mutedFg)
                ->LineHeight(1.f);
        case 1:
            return StoryTxt(cx, Str(s.market), 16, th.blue)->LineHeight(1.f);
        case 2:
            // The table clips the cell to its column, so a long name is cut
            // where the column ends however wide it has been dragged.
            return StoryTxt(cx, Str(s.name), 16, th.foreground)
                ->LineHeight(1.f);
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
            // render_percent: the percentage is tinted over the whole cell,
            // the way a ticker table does, at 5% of the trend color.
            return Div(cx->a)
                ->FlexRow()
                ->W(kFill)
                ->H(kFill)
                ->ItemsCenter()
                ->JustifyEnd()
                ->Bg(RgbaOpacity(trend, 0.05f))
                ->Child(StoryTxt(cx, Str(s.pct), 16, trend)->LineHeight(1.f));
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
    StoryToolbarOpt sizeRows[kNSizes];
    for (int i = 0; i < kNSizes; i++) {
        sizeRows[i].label = kSizeLabels[i];
        sizeRows[i].checked = self->size == i;
        sizeRows[i].act = DtActSize + i;
    }
    group->Child(StoryToolbarDropdown(
        cx, StrL("dt-size"), StoryFmt(cx, "Size: %s", kSizeLabels[self->size]),
        self->openMenu == DtMenuSize, ListenerArg(openMenu, DtMenuSize),
        sizeRows, kNSizes, act));
    group->Child(StoryToolbarDivider(cx));
    StoryToolbarOpt rowRows[kNRowCounts];
    for (int i = 0; i < kNRowCounts; i++) {
        rowRows[i].label = kRowLabels[i];
        rowRows[i].checked = self->rowCount == i;
        rowRows[i].act = DtActRows + i;
    }
    group->Child(StoryToolbarDropdown(
        cx, StrL("dt-rows"),
        StoryFmt(cx, "Rows: %d", kRowCounts[self->rowCount]),
        self->openMenu == DtMenuRows, ListenerArg(openMenu, DtMenuRows),
        rowRows, kNRowCounts, act));
    group->Child(StoryToolbarDivider(cx));
    StoryToolbarOpt extraRows[kNExtraCounts];
    for (int i = 0; i < kNExtraCounts; i++) {
        extraRows[i].label = kExtraLabels[i];
        extraRows[i].checked = self->extra == i;
        extraRows[i].act = DtActExtra + i;
    }
    group->Child(StoryToolbarDropdown(
        cx, StrL("dt-extra"),
        StoryFmt(cx, "Extra Columns: %d", kExtraCounts[self->extra]),
        self->openMenu == DtMenuExtra, ListenerArg(openMenu, DtMenuExtra),
        extraRows, kNExtraCounts, act));
    group->Child(StoryToolbarDivider(cx));
    // The Options menu, with the two separators Rust puts in it and the
    // Clear Selection row under the last one.
    StoryToolbarOpt optRows[DtOptCount + 1];
    for (int i = 0; i < DtOptCount; i++) {
        optRows[i].label = kDtOptions[i];
        optRows[i].checked = self->options[i];
        optRows[i].act = DtActOption + i;
        optRows[i].sep = i == DtOptStriped;
    }
    optRows[DtOptCount].label = "Clear Selection";
    optRows[DtOptCount].act = DtActClear;
    optRows[DtOptCount].plain = true;
    optRows[DtOptCount].sep = true;
    group->Child(StoryToolbarDropdown(cx, StrL("dt-options"), StrL("Options"),
                                      self->openMenu == DtMenuOptions,
                                      ListenerArg(openMenu, DtMenuOptions),
                                      optRows, DtOptCount + 1, act));
    group->Child(StoryToolbarDivider(cx));
    StoryToolbarOpt goRows[kNGoTo];
    for (int i = 0; i < kNGoTo; i++) {
        goRows[i].label = kGoToRows[i];
        goRows[i].act = DtActGoTo + i;
        goRows[i].plain = true;
        goRows[i].sep = i == 2;
    }
    group->Child(StoryToolbarDropdown(
        cx, StrL("dt-go-to"), StrL("Go To"), self->openMenu == DtMenuGoTo,
        ListenerArg(openMenu, DtMenuGoTo), goRows, kNGoTo, act));
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

    // Column widths, in the order the Rust delegate declares them. They are
    // what a column starts at; dragging its right edge makes the width the
    // table's own.
    const float wId = 45, wMarket = 61, wName = 176, wSymbol = 101,
                wPrice = 100, wChg = 100, wPct = 110;

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
    TableState* st = self->table.Get(cx);
    if (st) {
        st->loopSelection = self->options[DtOptLoop];
        st->colResizable = self->options[DtOptColResize];
        // col_movable: whether a head can be dragged into another place.
        st->colMovable = self->options[DtOptColOrder];
        st->sortable = self->options[DtOptSortable];
        st->colSelectable = self->options[DtOptColSelect];
        st->rowSelectable = self->options[DtOptRowSelect];
        st->cellSelectable = self->options[DtOptCellSelect];
        st->rowHeader = self->options[DtOptRowHeader];
        st->loading = self->options[DtOptLoading];
        // lazy_load: has_more, so the table asks for another page when the
        // last rows it built come near the end.
        st->hasMore = self->options[DtOptLazyLoad];
        st->onEvent = Listen(cx, &OnTableEvent);
    }

    // The grouped header spans the first four columns, so it has to follow
    // them when one of them is dragged wider.
    float wStock = 0;
    for (int i = 0; i < 4; i++) {
        int c = st ? TableColAt(st, i) : i;
        wStock +=
            st ? TableColWidth(st, c, kColumns[c].width) : kColumns[c].width;
    }

    // Two levels of grouped headers over the column row.
    El* group1 = Div(a)->FlexRow()->W(kFill)->BorderB(1, th.border);
    group1->Child(Div(a)
                      ->FlexRow()
                      ->W(wStock)
                      ->PadY(6)
                      ->JustifyCenter()
                      ->BorderR(1, th.border)
                      ->Child(StoryTxt(cx, StrL("Stock"), 16, th.foreground)
                                  ->LineHeight(1.f)));
    group1->Child(Div(a)->Grow()->PadY(6));
    El* group2 = Div(a)->FlexRow()->W(kFill)->BorderB(1, th.border);
    group2->Child(Div(a)
                      ->FlexRow()
                      ->W(wStock)
                      ->PadY(6)
                      ->JustifyCenter()
                      ->BorderR(1, th.border)
                      ->Child(StoryTxt(cx, StrL("Identity"), 16, th.foreground)
                                  ->LineHeight(1.f)));
    group2->Child(Div(a)->Grow()->FlexRow()->PadY(6)->JustifyCenter()->Child(
        StoryTxt(cx, StrL("Price & Change"), 16, th.foreground)
            ->LineHeight(1.f)));

    component::DataTable* table =
        component::DataTable::New(cx, StrL("data-table"), self->table)
            ->Columns(kColumns, nColumns)
            ->Rows(kRowCounts[self->rowCount], self, DtCellFor)
            ->H(520)
            ->RowHeight(kSizeRowH[self->size])
            ->Stripe(self->options[DtOptStriped]);
    if (self->options[DtOptGroupHeaders]) {
        table->GroupHeader(group1)->GroupHeader(group2);
    }
    El* box = table->IntoEl();

    // The status line under the table: min_h_9, px_3, muted at 35%, text_xs.
    VirtualRange vis = VirtualListVisibleRows(kRowCounts[self->rowCount],
                                              kSizeRowH[self->size],
                                              st ? st->scrollY : 0, 520);
    El* status = Div(a)
                     ->FlexRow()
                     ->W(kFill)
                     ->MinH(36)
                     ->PadX(12)
                     ->Gap(12)
                     ->JustifyBetween()
                     ->ItemsCenter()
                     ->Bg(RgbaOpacity(th.muted, 0.35f))
                     ->Font(12)
                     ->Fg(th.mutedFg);
    status->Child(TextEl(a, StoryFmt(cx, "Total · %d rows · %d columns",
                                     kRowCounts[self->rowCount],
                                     nColumns + kExtraCounts[self->extra])));
    El* right = Div(a)->FlexRow()->Gap(8)->ItemsCenter()->JustifyEnd();
    if (self->options[DtOptLoading]) {
        right->Child(
            component::Spinner::New(cx)->WithSize(UiSize::XSmall)->IntoEl());
    }
    right->Child(TextEl(a, StoryFmt(cx, "Current · rows %d..%d · columns 0..%d",
                                    vis.first, vis.end, nColumns - 1)));
    if (st && st->selectedCellRow >= 0) {
        right->Child(TextEl(a, StoryFmt(cx, "· cell %d:%d", st->selectedCellRow,
                                        st->selectedCellCol)));
    }
    if (st && !st->hasMore) {
        right->Child(TextEl(a, StrL("· complete")));
    }
    status->Child(right);
    page->Child(box);
    page->Child(status);
    if (self->message.s) {
        page->Child(StoryTxt(cx, self->message, 14, th.mutedFg));
    }
    return page;
}

STORY_PAGE_KEYS(StoryDataTable, DataTableStory);
