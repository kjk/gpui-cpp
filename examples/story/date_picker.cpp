#include "Story.h"

// The pickers on the page, in the order the sections list them.
enum {
    DpDefault = 0,
    DpInterval,
    DpRange7,
    DpCustom,
    DpDateRange,
    DpEmptyRange,
    DpYearRange,
    DpNoAppearance,
    DpCount
};

struct DpDate {
    int y = 0;
    int m = 0;
    int d = 0; // 0: no date
    int y2 = 0;
    int m2 = 0;
    int d2 = 0; // the end of a range
};

struct DatePickerStory {
    DpDate dates[DpCount] = {};
    int open = -1;
    // format!("Value: {:?}") of an Option<String>.
    char value[64] = "None";
    StoryToolbarState toolbar;
    bool seeded = false;

    static El* Render(DatePickerStory* self, Ctx* cx);
    static void OnKey(DatePickerStory* self, Ctx* cx, const KeyEvent* ev);
};

static void SetDate(DpDate* dst, LocalDate st) {
    dst->y = st.year;
    dst->m = st.month;
    dst->d = st.day;
}

static void TogglePicker(DatePickerStory* self, Ctx* cx, const ClickEvent*,
                         intptr_t i) {
    self->open = self->open == (int)i ? -1 : (int)i;
    Notify(cx);
}
static void ClearPicker(DatePickerStory* self, Ctx* cx, const ClickEvent*,
                        intptr_t i) {
    self->dates[i].d = 0;
    self->dates[i].y2 = 0;
    if (i == DpDefault || i == DpDateRange || i == DpEmptyRange) {
        StrCopyZ(self->value, (int)sizeof(self->value), "None");
    }
    Notify(cx);
}
static void PickDay(DatePickerStory* self, Ctx* cx, const ClickEvent*,
                    intptr_t day) {
    int i = self->open;
    if (i < 0) {
        return;
    }
    self->dates[i].d = (int)day;
    self->open = -1;
    if (i == DpDefault || i == DpDateRange || i == DpEmptyRange) {
        // The story subscribes to these three and prints the new date.
        snprintf(self->value, sizeof(self->value), "Some(\"%d-%02d-%02d\")",
                 self->dates[i].y, self->dates[i].m, self->dates[i].d);
    }
    Notify(cx);
}

static El* Picker(DatePickerStory* self, Ctx* cx, int i, Listener toggle,
                  Listener clear, Listener pick) {
    const DpDate& dt = self->dates[i];
    component::DatePicker* p = component::DatePicker::New(cx)
                                   ->Year(dt.y)
                                   ->Month(dt.m)
                                   ->Day(dt.d)
                                   ->W(280)
                                   ->Open(self->open == i)
                                   ->OnToggle(ListenerArg(toggle, i))
                                   ->OnClear(ListenerArg(clear, i))
                                   ->OnDay(pick);
    if (dt.y2 > 0) {
        p->RangeEnd(dt.y2, dt.m2, dt.d2);
    }
    return p->IntoEl();
}

El* DatePickerStory::Render(DatePickerStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    if (!self->seeded) {
        self->seeded = true;
        LocalDate now = DateToday();
        SetDate(&self->dates[DpDefault], now);
        SetDate(&self->dates[DpInterval], now);
        // chrono checked_add_days, the way the Rust story seeds the range.
        SetDate(&self->dates[DpRange7], DateAddDays(now, -1));
        SetDate(&self->dates[DpCustom], now);
        SetDate(&self->dates[DpDateRange], now);
        LocalDate end = DateAddDays(now, 4);
        self->dates[DpDateRange].y2 = end.year;
        self->dates[DpDateRange].m2 = end.month;
        self->dates[DpDateRange].d2 = end.day;
    }
    Listener toggle = Listen(cx, &TogglePicker);
    Listener clear = Listen(cx, &ClearPicker);
    Listener pick = Listen(cx, &PickDay);

    El* page = Div(a)->FlexCol()->Gap(12)->W(kFill);
    page->Child(StoryToolbar(cx, self));

    El* def = StorySection(
        cx, "Default", "Single-date selection with presets and clear action.");
    El* defCol = Div(a)->FlexCol()->Gap(12)->ItemsCenter();
    defCol->Child(component::DatePicker::New(cx)
                      ->Year(self->dates[DpDefault].y)
                      ->Month(self->dates[DpDefault].m)
                      ->Day(self->dates[DpDefault].d)
                      ->W(280)
                      ->Cleanable()
                      ->Open(self->open == DpDefault)
                      ->OnToggle(ListenerArg(toggle, DpDefault))
                      ->OnClear(ListenerArg(clear, DpDefault))
                      ->OnDay(pick)
                      ->IntoEl());
    defCol->Child(
        StoryTxt(cx, StoryFmt(cx, "Value: %s", self->value), 14, th.mutedFg));
    StorySectionAdd(def, defCol);
    page->Child(def);

    El* dis =
        StorySection(cx, "Disabled dates",
                     "Matchers can block intervals, ranges, or custom dates.");
    El* disCol = Div(a)->FlexCol()->Gap(12);
    disCol->Child(Picker(self, cx, DpInterval, toggle, clear, pick));
    // The second picker formats as %Y-%m-%d.
    const DpDate& r7 = self->dates[DpRange7];
    disCol->Child(component::DatePicker::New(cx)
                      ->Year(r7.y)
                      ->Month(r7.m)
                      ->Day(r7.d)
                      ->W(280)
                      ->Format(component::DateFormat::Dash)
                      ->Open(self->open == DpRange7)
                      ->OnToggle(ListenerArg(toggle, DpRange7))
                      ->OnClear(ListenerArg(clear, DpRange7))
                      ->OnDay(pick)
                      ->IntoEl());
    disCol->Child(Picker(self, cx, DpCustom, toggle, clear, pick));
    StorySectionAdd(dis, disCol);
    page->Child(dis);

    El* range =
        StorySection(cx, "Date range", "Two months with range presets.");
    StorySectionAdd(range, component::DatePicker::New(cx)
                               ->Year(self->dates[DpDateRange].y)
                               ->Month(self->dates[DpDateRange].m)
                               ->Day(self->dates[DpDateRange].d)
                               ->RangeEnd(self->dates[DpDateRange].y2,
                                          self->dates[DpDateRange].m2,
                                          self->dates[DpDateRange].d2)
                               ->W(280)
                               ->Cleanable()
                               ->Open(self->open == DpDateRange)
                               ->OnToggle(ListenerArg(toggle, DpDateRange))
                               ->OnClear(ListenerArg(clear, DpDateRange))
                               ->OnDay(pick)
                               ->IntoEl());
    page->Child(range);

    El* empty = StorySection(cx, "Empty range", "Empty range with presets.");
    StorySectionAdd(empty, component::DatePicker::New(cx)
                               ->Day(self->dates[DpEmptyRange].d)
                               ->Placeholder(StrL("Range mode picker"))
                               ->W(280)
                               ->Cleanable()
                               ->Open(self->open == DpEmptyRange)
                               ->OnToggle(ListenerArg(toggle, DpEmptyRange))
                               ->OnClear(ListenerArg(clear, DpEmptyRange))
                               ->OnDay(pick)
                               ->IntoEl());
    page->Child(empty);

    El* year = StorySection(cx, "Year range", "Custom year range.");
    StorySectionAdd(year, component::DatePicker::New(cx)
                              ->Day(self->dates[DpYearRange].d)
                              ->Placeholder(StrL("Select birthday"))
                              ->W(280)
                              ->Cleanable()
                              ->Open(self->open == DpYearRange)
                              ->OnToggle(ListenerArg(toggle, DpYearRange))
                              ->OnClear(ListenerArg(clear, DpYearRange))
                              ->OnDay(pick)
                              ->IntoEl());
    page->Child(year);

    El* style = StorySection(cx, "Custom style", "Appearance-free input.");
    StorySectionAdd(
        style, Div(a)
                   ->W(280)
                   ->Bg(th.secondary)
                   ->Child(component::DatePicker::New(cx)
                               ->Day(self->dates[DpNoAppearance].d)
                               ->Placeholder(StrL("Without appearance"))
                               ->W(280)
                               ->Appearance(false)
                               ->Open(self->open == DpNoAppearance)
                               ->OnToggle(ListenerArg(toggle, DpNoAppearance))
                               ->OnDay(pick)
                               ->IntoEl()));
    page->Child(style);
    return page;
}

// Esc closes what this page has open, like an overlay dismiss.
void DatePickerStory::OnKey(DatePickerStory* self, Ctx* cx,
                            const KeyEvent* ev) {
    if (ev->vk != KeyEscape) {
        return;
    }
    self->open = -1;
    Notify(cx);
}

STORY_PAGE_KEYS(StoryDatePicker, DatePickerStory);
