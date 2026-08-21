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

// The Options dropdown, in Rust's own order.
enum {
    DtOptLoop = 0,
    DtOptFixedColumn,
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
    "Loop Selection", "Fixed Column",      "Column Resize",  "Column Order",
    "Sortable",       "Column Selectable", "Row Selectable", "Cell Selectable",
    "Row Header",     "Striped Rows",      "Loading",        "Lazy Load",
    "Refresh Data",   "Group Headers"};
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
    bool options[DtOptCount] = {false, true,  true,  true,  true,  true,  true,
                                false, false, false, false, false, false, true};
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
        case TableEventKind::SelectCell:
            self->message =
                StrDup(fmt("Selected cell %d:%d", ev->row, ev->col));
            break;
        case TableEventKind::DoubleClickedRow:
            self->message = StrDup(fmt("Double clicked row %d", ev->row));
            break;
        case TableEventKind::DoubleClickedCell:
            self->message =
                StrDup(fmt("Double clicked cell %d:%d", ev->row, ev->col));
            break;
        // The story prints these; here they say the same thing on the status
        // line. A row event with -1 is Rust's RightClickedRow(None), which
        // says the mark has gone rather than that a row was clicked.
        case TableEventKind::RightClickedRow:
            self->message = ev->row >= 0
                                ? StrDup(fmt("Right clicked row %d", ev->row))
                                : StrDup(StrL("Right click cleared"));
            break;
        case TableEventKind::RightClickedCell:
            self->message =
                StrDup(fmt("Right clicked cell %d:%d", ev->row, ev->col));
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

// The base columns' count, which is where the "Column N" extras start.
static const int kBaseColumns = 45;

// How a column past the sixth reads. Rust hangs a formatter off each field;
// the fields are generated here, so the kind is the whole difference.
enum class DtCell : uint8_t {
    Compact, // compact(): 1.2M rather than 1234567
    Fixed2,  // {:.2}
    Int,     // {:.0}
    Rate,    // {:.2}% off a 0..1 rate, in the plain colour
    Percent, // render_percent: tinted over the cell
    Change   // render_change: signed, in the trend colour
};

static const DtCell kCellKinds[kBaseColumns - 7] = {
    DtCell::Compact, DtCell::Compact, DtCell::Compact, DtCell::Compact,
    DtCell::Int,     DtCell::Int,     DtCell::Percent, DtCell::Fixed2,
    DtCell::Compact, DtCell::Fixed2,  DtCell::Compact, DtCell::Fixed2,
    DtCell::Fixed2,  DtCell::Fixed2,  DtCell::Fixed2,  DtCell::Rate,
    DtCell::Rate,    DtCell::Rate,    DtCell::Fixed2,  DtCell::Fixed2,
    DtCell::Fixed2,  DtCell::Fixed2,  DtCell::Fixed2,  DtCell::Fixed2,
    DtCell::Compact, DtCell::Percent, DtCell::Change,  DtCell::Compact,
    DtCell::Percent, DtCell::Change,  DtCell::Compact, DtCell::Compact,
    DtCell::Compact, DtCell::Int,     DtCell::Int,     DtCell::Int,
    DtCell::Int,     DtCell::Int};

// The Rust delegate fakes every field per row; a hash of the cell does the
// same job here, and gives the same cell the same value every frame.
static float DtNoise(int row, int col) {
    uint32_t h = (uint32_t)row * 2654435761u ^ (uint32_t)col * 2246822519u;
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    return (float)(h % 100000u) / 100000.f;
}

// compact(): the shortest reading of a large number.
static Str DtCompact(Ctx* cx, float v) {
    if (v >= 1e9f) {
        return StoryFmt(cx, "%.2fB", (double)(v / 1e9f));
    }
    if (v >= 1e6f) {
        return StoryFmt(cx, "%.2fM", (double)(v / 1e6f));
    }
    if (v >= 1e3f) {
        return StoryFmt(cx, "%.2fK", (double)(v / 1e3f));
    }
    return StoryFmt(cx, "%.2f", (double)v);
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
        case 6:
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
        default:
            break;
    }
    if (col >= kBaseColumns) {
        // The delegate has no field for a "Column N", so every one of them
        // reads the same.
        return StoryTxt(cx, StrL("--"), 16, th.mutedFg)->LineHeight(1.f);
    }
    float n = DtNoise(row, col);
    switch (kCellKinds[col - 7]) {
        case DtCell::Compact:
            return StoryTxt(cx, DtCompact(cx, n * 1e9f), 16, th.foreground)
                ->LineHeight(1.f);
        case DtCell::Int:
            return StoryTxt(cx, StoryFmt(cx, "%.0f", (double)(n * 1000.f)), 16,
                            th.foreground)
                ->LineHeight(1.f);
        case DtCell::Rate:
            return StoryTxt(cx, StoryFmt(cx, "%.2f%%", (double)(n * 100.f)), 16,
                            th.foreground)
                ->LineHeight(1.f);
        case DtCell::Change: {
            float v = (n - 0.5f) * 200.f;
            return StoryTxt(cx, StoryFmt(cx, "%+.2f", (double)v), 16,
                            v >= 0 ? th.green : th.red)
                ->LineHeight(1.f);
        }
        case DtCell::Percent: {
            float v = (n - 0.5f) * 20.f;
            Rgba c = v >= 0 ? th.green : th.red;
            return Div(cx->a)
                ->FlexRow()
                ->W(kFill)
                ->H(kFill)
                ->ItemsCenter()
                ->JustifyEnd()
                ->Bg(RgbaOpacity(c, 0.05f))
                ->Child(StoryTxt(cx, StoryFmt(cx, "%+.2f%%", (double)v), 16, c)
                            ->LineHeight(1.f));
        }
        default:
            return StoryTxt(cx, StoryFmt(cx, "%.2f", (double)(n * 1000.f)), 16,
                            th.foreground)
                ->LineHeight(1.f);
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

    // The columns the Rust delegate declares, in its own order. The first
    // four are .fixed(ColumnFixed::Left); everything after them scrolls. The
    // width is what a column starts at — dragging its right edge makes the
    // width the table's own. Fields: title, width, right, sortable,
    // selectable, resizable, fixed.
    static const component::TableColumn kColumns[] = {
        {StrL("ID"), 60, false, false, false, true, true},
        {StrL("Market"), 60, false, false, true, true, true},
        {StrL("Name"), 180, false, false, true, true, true},
        {StrL("Symbol"), 100, false, true, true, true, true},
        {StrL("Price"), 100, true, true, true},
        {StrL("Chg"), 100, true, true, true},
        {StrL("Chg%"), 110, true, true, true},
        {StrL("Volume"), 100, true, false, true},
        {StrL("Turnover"), 100, true, false, true},
        {StrL("Market Cap"), 110, true, false, true},
        {StrL("TTM"), 100, true, false, true},
        {StrL("5m Ranking"), 110, true, false, true},
        {StrL("60d Ranking"), 110, true, false, true},
        {StrL("Year Chg%"), 110, true, false, true},
        {StrL("Bid"), 100, true, false, true},
        {StrL("Bid Vol"), 100, true, false, true},
        {StrL("Ask"), 100, true, false, true},
        {StrL("Ask Vol"), 100, true, false, true},
        {StrL("Open"), 100, true, false, true},
        {StrL("Prev Close"), 110, true, false, true},
        {StrL("High"), 100, true, false, true},
        {StrL("Low"), 100, true, false, true},
        {StrL("Turnover Rate"), 120, true, false, true},
        {StrL("Rise Rate"), 100, true, false, true},
        {StrL("Amplitude"), 110, true, false, true},
        {StrL("P/E"), 100, true, false, true},
        {StrL("P/B"), 100, true, false, true},
        {StrL("Volume Ratio"), 120, true, false, true},
        {StrL("Bid Ask Ratio"), 120, true, false, true},
        {StrL("Latest Pre Close"), 140, true, false, true},
        {StrL("Latest Post Close"), 140, true, false, true},
        {StrL("Pre Mkt Cap"), 120, true, false, true},
        {StrL("Pre Mkt%"), 100, true, false, true},
        {StrL("Pre Mkt Chg"), 120, true, false, true},
        {StrL("Post Mkt Cap"), 120, true, false, true},
        {StrL("Post Mkt%"), 110, true, false, true},
        {StrL("Post Mkt Chg"), 120, true, false, true},
        {StrL("Float Cap"), 110, true, false, true},
        {StrL("Shares"), 100, true, false, true},
        {StrL("Float Shares"), 120, true, false, true},
        {StrL("5d Ranking"), 110, true, false, true},
        {StrL("10d Ranking"), 110, true, false, true},
        {StrL("30d Ranking"), 110, true, false, true},
        {StrL("120d Ranking"), 120, true, false, true},
        {StrL("250d Ranking"), 120, true, false, true},
    };
    const int nBase = (int)(sizeof(kColumns) / sizeof(kColumns[0]));
    // columns_count(): the delegate's own plus however many extras the
    // toolbar asked for, each of which the delegate names "Column N".
    const int nExtra = kExtraCounts[self->extra];
    const int nColumns = nBase + nExtra;
    auto* cols = (component::TableColumn*)Alloc(
        a, (int)sizeof(component::TableColumn) * nColumns);
    for (int i = 0; i < nBase; i++) {
        cols[i] = kColumns[i];
    }
    for (int i = 0; i < nExtra; i++) {
        cols[nBase + i] = {StoryFmt(cx, "Column %d", i + 1), 100, false, false,
                           true};
    }
    TableState* st = self->table.Get(cx);
    if (st) {
        st->loopSelection = self->options[DtOptLoop];
        st->colFixed = self->options[DtOptFixedColumn];
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

    // group_headers: two levels of bands, each spanning every column, the
    // lower one subdividing the upper. The trailing band takes the extra
    // "Column N" columns with it, so both rows still cover the table.
    const int trailing = kExtraCounts[self->extra];
    const component::TableGroupCell kGroup1[] = {
        {StrL("Stock"), 4},          {StrL("Market Data"), 10},
        {StrL("Quotes"), 8},         {StrL("Stats"), 7},
        {StrL("Extended Hours"), 8}, {StrL("Shares & Rankings"), 8 + trailing},
    };
    const component::TableGroupCell kGroup2[] = {
        {StrL("Identity"), 4},   {StrL("Price & Change"), 3},
        {StrL("Turnover"), 4},   {StrL("Momentum"), 3},
        {StrL("Order Book"), 4}, {StrL("Session"), 4},
        {StrL("Activity"), 3},   {StrL("Valuation"), 2},
        {StrL("Ratios"), 2},     {StrL("Pre & Post Market"), 8},
        {StrL("Shares"), 3},     {StrL("Rankings"), 5 + trailing},
    };

    component::DataTable* table =
        component::DataTable::New(cx, StrL("data-table"), self->table)
            ->Columns(cols, nColumns)
            ->Rows(kRowCounts[self->rowCount], self, DtCellFor)
            ->H(520)
            ->RowHeight(kSizeRowH[self->size])
            ->Stripe(self->options[DtOptStriped]);
    if (self->options[DtOptGroupHeaders]) {
        table->GroupHeader(kGroup1, 6)->GroupHeader(kGroup2, 12);
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
                                     kRowCounts[self->rowCount], nColumns)));
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

STORY_PAGE(StoryDataTable, DataTableStory);
