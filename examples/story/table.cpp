#include "Story.h"

struct Invoice {
    const char* id;
    const char* status;
    const char* method;
    const char* method2; // the second line of a two-line method cell
    const char* amount;
    const char* date;
};

static const Invoice kInvoices[] = {
    {"INV001", "Paid", "Credit Card", nullptr, "$250.00", "2024-01-15"},
    {"INV002", "Pending", "PayPal", nullptr, "$150.00", "2024-02-01"},
    {"INV003", "Unpaid", "Bank Transfer", nullptr, "$350.00", "2024-02-15"},
    {"INV004", "Paid", "Credit Card", "Master Card / Visa", "$450.00",
     "2024-03-01"},
    {"INV005", "Paid", "PayPal", nullptr, "$550.00", "2024-03-15"},
    {"INV006", "Pending", "Bank Transfer", nullptr, "$200.00", "2024-04-01"},
    {"INV007", "Unpaid", "Credit Card", nullptr, "$300.00", "2024-04-15"},
};

struct TableStory {
    StoryToolbarState toolbar;

    static El* Render(TableStory* self, Ctx* cx);
};

// status_tag(): an xsmall outline tag in the status color.
static El* StatusTag(Ctx* cx, const char* status) {
    component::Tag* tag = component::Tag::New(cx, Str(status))
                              ->Outline()
                              ->WithSize(UiSize::XSmall);
    if (strcmp(status, "Paid") == 0) {
        tag->Success();
    } else if (strcmp(status, "Pending") == 0) {
        tag->Warning();
    } else if (strcmp(status, "Unpaid") == 0) {
        tag->Danger();
    }
    return tag->IntoEl();
}

static El* Cell(Ctx* cx, float w, bool right) {
    Arena* a = cx->a;
    El* c = Div(a)->FlexRow()->PadX(8)->PadY(8)->ItemsCenter();
    if (w > 0) {
        c->W(w);
    } else {
        c->Grow();
    }
    if (right) {
        c->JustifyEnd();
    }
    return c;
}

// Table text sits in a tight line box, so a row is py_2 plus the text.
static El* HeadCell(Ctx* cx, const char* text, float w, bool right) {
    El* c = Cell(cx, w, right);
    c->Child(StoryTxt(cx, Str(text), 14, cx->theme().mutedFg)->LineHeight(1.f));
    return c;
}

static El* TextCell(Ctx* cx, const char* text, float w, bool right) {
    El* c = Cell(cx, w, right);
    c->Child(StoryTxt(cx, Str(text), 16, cx->theme().foreground)
                 ->LineHeight(1.f));
    return c;
}

El* TableStory::Render(TableStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    const int nInvoices = (int)(sizeof(kInvoices) / sizeof(kInvoices[0]));
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(cx, self));

    El* def = StorySection(cx, "Default", nullptr);
    El* table = Div(a)->FlexCol()->W(kFill);
    El* head = Div(a)->FlexRow()->W(kFill)->BorderB(1, th.border);
    // The Rust table shares the width across its columns, so the w(150) the
    // story asks for lands nearer 100.
    head->Child(HeadCell(cx, "Invoice", 100, false));
    head->Child(HeadCell(cx, "Status", 0, false));
    head->Child(HeadCell(cx, "Amount", 120, true));
    head->Child(HeadCell(cx, "Date", 120, true));
    table->Child(head);
    for (int i = 0; i < nInvoices; i++) {
        const Invoice& inv = kInvoices[i];
        El* row = Div(a)->FlexRow()->W(kFill)->BorderB(1, th.border);
        row->Child(TextCell(cx, inv.id, 100, false));
        El* statusCell = Cell(cx, 144, false);
        statusCell->Child(StatusTag(cx, inv.status));
        row->Child(statusCell);
        El* methodCell = Cell(cx, 0, false);
        El* methodCol = Div(a)->FlexCol();
        methodCol->Child(StoryTxt(cx, Str(inv.method), 16, th.foreground)
                             ->LineHeight(1.4f));
        if (inv.method2) {
            methodCol->Child(StoryTxt(cx, Str(inv.method2), 16, th.foreground)
                                 ->LineHeight(1.4f));
        }
        methodCell->Child(methodCol);
        row->Child(methodCell);
        row->Child(TextCell(cx, inv.amount, 120, true));
        row->Child(TextCell(cx, inv.date, 120, true));
        table->Child(row);
    }
    // The footer spans the first three columns, then the total.
    El* foot = Div(a)->FlexRow()->W(kFill)->Bg(th.tokens.muted);
    foot->Child(TextCell(cx, "Total", 0, false));
    foot->Child(TextCell(cx, "$2,250.00", 240, true));
    table->Child(foot);
    table->Child(Div(a)->W(kFill)->PadY(16)->FlexRow()->JustifyCenter()->Child(
        StoryTxt(cx, StrL("A list of your recent invoices."), 16, th.mutedFg)));
    StorySectionAdd(def, table);
    page->Child(def);

    El* bordered = StorySection(cx, "Bordered", nullptr);
    El* box = Div(a)->FlexCol()->W(kFill)->ClipY()->Radius(th.radius)->Border(
        1, th.border);
    El* bhead = Div(a)->FlexRow()->W(kFill)->BorderB(1, th.border);
    bhead->Child(HeadCell(cx, "Invoice", 100, false));
    bhead->Child(HeadCell(cx, "Method", 0, false));
    bhead->Child(HeadCell(cx, "Amount", 120, true));
    bhead->Child(HeadCell(cx, "Date", 120, true));
    box->Child(bhead);
    for (int i = 0; i < 6; i++) {
        const Invoice& inv = kInvoices[i];
        El* row = Div(a)->FlexRow()->W(kFill)->BorderB(1, th.border);
        if (i % 2 != 0) {
            row->Bg(th.tokens.tableEven);
        }
        row->Child(TextCell(cx, inv.id, 100, false));
        El* methodCell = Cell(cx, 0, false);
        El* methodCol = Div(a)->FlexCol();
        methodCol->Child(StoryTxt(cx, Str(inv.method), 16, th.foreground)
                             ->LineHeight(1.4f));
        if (inv.method2) {
            methodCol->Child(StoryTxt(cx, Str(inv.method2), 16, th.foreground)
                                 ->LineHeight(1.4f));
        }
        methodCell->Child(methodCol);
        row->Child(methodCell);
        row->Child(TextCell(cx, inv.amount, 120, true));
        row->Child(TextCell(cx, inv.date, 120, true));
        box->Child(row);
    }
    StorySectionAdd(bordered, box);
    page->Child(bordered);
    return page;
}

STORY_PAGE(StoryTable, TableStory);
