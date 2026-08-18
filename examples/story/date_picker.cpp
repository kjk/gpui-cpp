#include "Story.h"

struct DatePickerStory {
    int calYear = 2026;
    int calMonth = 8;
    int calDay = 17;
    bool dateOpen = false;
    static El* Render(DatePickerStory* self, Ctx* cx);
    static void OnKey(DatePickerStory* self, Ctx* cx, const KeyEvent* ev);
};

static void ToggleDate(DatePickerStory* self, Ctx* cx, const ClickEvent*) {
    self->dateOpen = !self->dateOpen;
}
static void PickDate(DatePickerStory* self, Ctx* cx, const ClickEvent*,
                     intptr_t d) {
    self->calDay = d;
    self->dateOpen = false;
}

El* DatePickerStory::Render(DatePickerStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(cx, "Default",
                           "A date picker component with range and presets.");
    StorySectionAdd(sec, component::DatePicker::New(cx)
                             ->Year(self->calYear)
                             ->Month(self->calMonth)
                             ->Day(self->calDay)
                             ->Open(self->dateOpen)
                             ->OnToggle(Listen(cx, &ToggleDate))
                             ->OnDay(Listen(cx, &PickDate))
                             ->IntoEl());
    page->Child(sec);

    El* dis = StorySection(cx, "Disabled dates", nullptr);
    StorySectionAdd(dis, component::DatePicker::New(cx)
                             ->Year(self->calYear)
                             ->Month(self->calMonth)
                             ->Day(self->calDay)
                             ->IntoEl());
    page->Child(dis);

    El* range = StorySection(cx, "Date range", nullptr);
    StorySectionAdd(range, component::DatePicker::New(cx)
                               ->Year(self->calYear)
                               ->Month(self->calMonth)
                               ->Day(self->calDay)
                               ->IntoEl());
    page->Child(range);

    El* empty = StorySection(cx, "Empty range", nullptr);
    StorySectionAdd(empty, component::DatePicker::New(cx)
                               ->Year(self->calYear)
                               ->Month(self->calMonth)
                               ->Day(0)
                               ->IntoEl());
    page->Child(empty);

    El* year = StorySection(cx, "Year range", nullptr);
    StorySectionAdd(year, component::DatePicker::New(cx)
                              ->Year(self->calYear)
                              ->Month(self->calMonth)
                              ->Day(self->calDay)
                              ->IntoEl());
    page->Child(year);

    El* style = StorySection(cx, "Custom style", nullptr);
    StorySectionAdd(style, component::DatePicker::New(cx)
                               ->Year(self->calYear)
                               ->Month(self->calMonth)
                               ->Day(self->calDay)
                               ->IntoEl());
    page->Child(style);
    return page;
}

// Esc closes what this page has open, like an overlay dismiss.
void DatePickerStory::OnKey(DatePickerStory* self, Ctx* cx,
                            const KeyEvent* ev) {
    if (ev->vk != VK_ESCAPE) {
        return;
    }
    self->dateOpen = false;
    Notify(cx);
}

STORY_PAGE_KEYS(StoryDatePicker, DatePickerStory);
