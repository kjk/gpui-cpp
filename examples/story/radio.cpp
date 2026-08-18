#include "Story.h"

struct RadioStory {
    int radioSel = 0;
    int radioBilling = 1;
    StoryToolbarState toolbar;

    static El* Render(RadioStory* self, Ctx* cx);
};

static void SetDel0(RadioStory* self, Ctx*, const ClickEvent*, intptr_t) {
    self->radioSel = 0;
}
static void SetDel1(RadioStory* self, Ctx*, const ClickEvent*, intptr_t) {
    self->radioSel = 1;
}
static void SetBill0(RadioStory* self, Ctx*, const ClickEvent*, intptr_t) {
    self->radioBilling = 0;
}
static void SetBill1(RadioStory* self, Ctx*, const ClickEvent*, intptr_t) {
    self->radioBilling = 1;
}
static void SetBill2(RadioStory* self, Ctx*, const ClickEvent*, intptr_t) {
    self->radioBilling = 2;
}

El* RadioStory::Render(RadioStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(12)->W(kFill);
    page->Child(StoryToolbar(cx, self));

    El* del = StorySection(cx, "Delivery",
                           "Choose one option from a clearly described set.");
    El* delCol = Div(a)->FlexCol()->Gap(12)->W(320);
    delCol->Child(component::Radio::New(cx, StrL("standard"))
                      ->Label(StrL("Standard delivery"))
                      ->Hint(StrL("Arrives in 3–5 business days."))
                      ->Checked(self->radioSel == 0)
                      ->WithSize(self->toolbar.size)
                      ->OnClick(Listen(cx, &SetDel0))
                      ->IntoEl());
    delCol->Child(component::Radio::New(cx, StrL("express"))
                      ->Label(StrL("Express delivery"))
                      ->Hint(StrL("Arrives the next business day."))
                      ->Checked(self->radioSel == 1)
                      ->WithSize(self->toolbar.size)
                      ->OnClick(Listen(cx, &SetDel1))
                      ->IntoEl());
    delCol->Child(component::Radio::New(cx, StrL("pickup"))
                      ->Label(StrL("Store pickup"))
                      ->Hint(StrL("Unavailable for this order."))
                      ->Disabled(true)
                      ->WithSize(self->toolbar.size)
                      ->IntoEl());
    StorySectionAdd(del, delCol);
    page->Child(del);

    El* bill =
        StorySection(cx, "Billing cycle",
                     "Horizontal groups work for short, related choices.");
    // RadioGroup::horizontal is h_flex().w_full().flex_wrap().gap_3(); the
    // justify_between the story asks for lands on the wrapper, which has one
    // child, so it never spreads them.
    El* billRow = Div(a)->FlexRow()->FlexWrap()->Gap(12)->W(320);
    billRow->Child(component::Radio::New(cx, StrL("monthly"))
                       ->Label(StrL("Monthly"))
                       ->Checked(self->radioBilling == 0)
                       ->WithSize(self->toolbar.size)
                       ->OnClick(Listen(cx, &SetBill0))
                       ->IntoEl());
    billRow->Child(component::Radio::New(cx, StrL("yearly"))
                       ->Label(StrL("Yearly"))
                       ->Checked(self->radioBilling == 1)
                       ->WithSize(self->toolbar.size)
                       ->OnClick(Listen(cx, &SetBill1))
                       ->IntoEl());
    billRow->Child(component::Radio::New(cx, StrL("lifetime"))
                       ->Label(StrL("Lifetime"))
                       ->Checked(self->radioBilling == 2)
                       ->WithSize(self->toolbar.size)
                       ->OnClick(Listen(cx, &SetBill2))
                       ->IntoEl());
    StorySectionAdd(bill, billRow);
    page->Child(bill);
    return page;
}

STORY_PAGE(StoryRadio, RadioStory);
