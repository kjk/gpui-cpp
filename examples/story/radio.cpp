#include "Story.h"

static void SetDel0(StoryApp* app, bool) {
    app->radioSel = 0;
}
static void SetDel1(StoryApp* app, bool) {
    app->radioSel = 1;
}
static void SetBill0(StoryApp* app, bool) {
    app->radioBilling = 0;
}
static void SetBill1(StoryApp* app, bool) {
    app->radioBilling = 1;
}
static void SetBill2(StoryApp* app, bool) {
    app->radioBilling = 2;
}

El* RadioRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(cx, app));

    El* del = StorySection(cx, "Delivery",
                           "Choose one option from a clearly described set.");
    El* delCol = Div(a)->FlexCol()->Gap(12)->W(320);
    delCol->Child(component::Radio::New(cx, StrL("standard"))
                      ->Label(StrL("Standard delivery"))
                      ->Hint(StrL("Arrives in 3–5 business days."))
                      ->Checked(app->radioSel == 0)
                      ->WithSize(app->size)
                      ->OnClick(MkFunc1(&SetDel0, app))
                      ->IntoEl());
    delCol->Child(component::Radio::New(cx, StrL("express"))
                      ->Label(StrL("Express delivery"))
                      ->Hint(StrL("Arrives the next business day."))
                      ->Checked(app->radioSel == 1)
                      ->WithSize(app->size)
                      ->OnClick(MkFunc1(&SetDel1, app))
                      ->IntoEl());
    delCol->Child(component::Radio::New(cx, StrL("pickup"))
                      ->Label(StrL("Store pickup"))
                      ->Hint(StrL("Unavailable for this order."))
                      ->Disabled(true)
                      ->WithSize(app->size)
                      ->IntoEl());
    StorySectionAdd(del, delCol);
    page->Child(del);

    El* bill =
        StorySection(cx, "Billing cycle",
                     "Horizontal groups work for short, related choices.");
    El* billRow = Div(a)->FlexRow()->Gap(16)->W(320)->JustifyBetween();
    billRow->Child(component::Radio::New(cx, StrL("monthly"))
                       ->Label(StrL("Monthly"))
                       ->Checked(app->radioBilling == 0)
                       ->WithSize(app->size)
                       ->OnClick(MkFunc1(&SetBill0, app))
                       ->IntoEl());
    billRow->Child(component::Radio::New(cx, StrL("yearly"))
                       ->Label(StrL("Yearly"))
                       ->Checked(app->radioBilling == 1)
                       ->WithSize(app->size)
                       ->OnClick(MkFunc1(&SetBill1, app))
                       ->IntoEl());
    billRow->Child(component::Radio::New(cx, StrL("lifetime"))
                       ->Label(StrL("Lifetime"))
                       ->Checked(app->radioBilling == 2)
                       ->WithSize(app->size)
                       ->OnClick(MkFunc1(&SetBill2, app))
                       ->IntoEl());
    StorySectionAdd(bill, billRow);
    page->Child(bill);
    return page;
}

void RadioClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryRadio, RadioRender, RadioClick);
