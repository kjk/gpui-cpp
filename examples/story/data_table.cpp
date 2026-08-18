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

struct DataTableStory {
    int rowCount = 2; // 5000
    int extra = 0;    // 0 extra columns
    int openMenu = 0;
    bool options[6] = {false, true, true, true, false, false};
    StoryToolbarState toolbar;

    static El* Render(DataTableStory* self, Ctx* cx);
};

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

static El* DtCell(Ctx* cx, float w, bool right) {
    Arena* a = cx->a;
    El* c = Div(a)->FlexRow()->W(w)->PadX(8)->PadY(6)->ItemsCenter();
    if (right) {
        c->JustifyEnd();
    }
    return c;
}

El* DataTableStory::Render(DataTableStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    Listener openMenu = Listen(cx, &DtMenuOpen);
    Listener act = Listen(cx, &DtMenuAct);
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
            ->PadX(10)
            ->ItemsCenter()
            ->JustifyCenter()
            ->HoverBg(th.muted)
            ->Child(StoryTxt(cx, StrL("Export CSV"), 12, th.foreground));
    group->Child(exportBtn);
    toolbarRow->Child(group);
    page->Child(toolbarRow);

    // Column widths, in the order the Rust delegate declares them.
    const float wId = 45, wMarket = 61, wName = 176, wSymbol = 101,
                wPrice = 100, wChg = 100, wPct = 110;
    El* box =
        Div(a)->FlexCol()->W(kFill)->Radius(th.radius)->Border(1, th.border);

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
    box->Child(group1);
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
    box->Child(group2);

    struct Column {
        const char* title;
        float w;
        bool right;
        bool sortable;
    };
    const Column columns[] = {
        {"ID", wId, false, false},     {"Market", wMarket, false, false},
        {"Name", wName, false, false}, {"Symbol", wSymbol, false, true},
        {"Price", wPrice, true, true}, {"Chg", wChg, true, true},
        {"Chg%", wPct, true, true},
    };
    El* head = Div(a)->FlexRow()->W(kFill)->BorderB(1, th.border);
    for (size_t c = 0; c < sizeof(columns) / sizeof(columns[0]); c++) {
        El* cell = DtCell(cx, columns[c].w, columns[c].right);
        if (c > 0) {
            cell->BorderL(1, th.border);
        }
        cell->Child(StoryTxt(cx, Str(columns[c].title), 14, th.foreground)
                        ->LineHeight(1.f));
        if (columns[c].sortable) {
            cell->JustifyBetween();
            cell->Child(IconEl(a, IconName::ChevronsUpDown, 12)
                            ->Fg(th.mutedFg));
        }
        head->Child(cell);
    }
    box->Child(head);

    const int nStocks = (int)(sizeof(kStocks) / sizeof(kStocks[0]));
    for (int i = 0; i < nStocks; i++) {
        const Stock& s = kStocks[i];
        Rgba trend = s.up ? th.green : th.red;
        El* row = Div(a)->FlexRow()->W(kFill)->BorderB(1, th.border);
        El* idCell = DtCell(cx, wId, false);
        idCell->Child(StoryTxt(cx, StoryFmt(cx, "%d", i), 16, th.mutedFg)
                          ->LineHeight(1.f));
        row->Child(idCell);
        El* marketCell = DtCell(cx, wMarket, false);
        marketCell->BorderL(1, th.border);
        marketCell
            ->Child(StoryTxt(cx, Str(s.market), 16, th.blue)->LineHeight(1.f));
        row->Child(marketCell);
        El* nameCell = DtCell(cx, wName, false);
        nameCell->BorderL(1, th.border);
        // The Rust cell hides its overflow; ours truncates the text instead.
        nameCell->Child(StoryTxt(cx, Str(s.name), 16, th.foreground)
                            ->LineHeight(1.f)
                            ->MaxW(wName - 16)
                            ->Truncate());
        row->Child(nameCell);
        El* symbolCell = DtCell(cx, wSymbol, false);
        symbolCell->BorderL(1, th.border);
        symbolCell->Child(StoryTxt(cx, Str(s.symbol), 16, th.foreground)
                              ->Medium()
                              ->LineHeight(1.f));
        row->Child(symbolCell);
        El* priceCell = DtCell(cx, wPrice, true);
        priceCell->BorderL(1, th.border);
        priceCell->Child(StoryTxt(cx, Str(s.price), 16, th.foreground)
                             ->LineHeight(1.f));
        row->Child(priceCell);
        El* chgCell = DtCell(cx, wChg, true);
        chgCell->BorderL(1, th.border);
        chgCell->Child(StoryTxt(cx, Str(s.chg), 16, trend)->LineHeight(1.f));
        row->Child(chgCell);
        El* pctCell = DtCell(cx, wPct, true);
        pctCell->BorderL(1, th.border)->Bg(RgbaOpacity(trend, 0.06f));
        pctCell->Child(StoryTxt(cx, Str(s.pct), 16, trend)->LineHeight(1.f));
        row->Child(pctCell);
        box->Child(row);
    }

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
    box->Child(status);
    page->Child(box);
    return page;
}

STORY_PAGE(StoryDataTable, DataTableStory);
