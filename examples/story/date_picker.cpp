#include "Story.h"

// The DatePickerState entities in the Rust story, in declaration order.
enum {
    DpDefault = 0,
    DpSmall,
    DpLarge,
    DpCustom,
    DpDateRange,
    DpEmptyRange,
    DpBirthday,
    DpNoAppearance,
    DpCount
};

struct PickerState {
    LocalDate start = {};
    LocalDate end = {};
    int viewYear = 0;
    int viewMonth = 0;
    CalendarView calendarView = CalendarView::Day;
    int yearMin = 0;
    int yearMax = 0;
    int yearPageStart = 0;
    bool range = false;
    DateMatcher disabled = {};
};

struct DatePickerStory {
    PickerState pickers[DpCount] = {};
    int open = -1;
    int focused = DpDefault;
    // format!("Value: {:?}") of the subscribed Option<String>.
    char value[96] = "None";
    StoryToolbarState toolbar;
    bool seeded = false;

    static El* Render(DatePickerStory* self, Ctx* cx);
    static void OnKey(DatePickerStory* self, Ctx* cx, const KeyEvent* ev);
};

static component::DateRangePreset gSinglePresets[3];
static component::DateRangePreset gRangePresets[4];

static bool DateIsSet(LocalDate date) {
    return date.year != 0 && date.month != 0 && date.day != 0;
}

static bool FirstFiveDays(LocalDate date) {
    // Rust's custom matcher is `date.day0() < 5`.
    return date.day <= 5;
}

static bool SubscribedPicker(int picker) {
    return picker == DpDefault || picker == DpDateRange ||
           picker == DpEmptyRange;
}

static void UpdateValue(DatePickerStory* self, int picker) {
    if (!SubscribedPicker(picker)) {
        return;
    }
    const PickerState& state = self->pickers[picker];
    if (!DateIsSet(state.start) || (state.range && !DateIsSet(state.end))) {
        StrCopyZ(self->value, (int)sizeof(self->value), "None");
        return;
    }
    if (state.range) {
        snprintf(self->value, sizeof(self->value),
                 "Some(\"%d-%02d-%02d - %d-%02d-%02d\")", state.start.year,
                 state.start.month, state.start.day, state.end.year,
                 state.end.month, state.end.day);
    } else {
        snprintf(self->value, sizeof(self->value), "Some(\"%d-%02d-%02d\")",
                 state.start.year, state.start.month, state.start.day);
    }
}

static void TogglePicker(DatePickerStory* self, Ctx* cx, const ClickEvent*,
                         intptr_t picker) {
    self->focused = (int)picker;
    self->open = self->open == (int)picker ? -1 : (int)picker;
    Notify(cx);
}

static void ClearPicker(DatePickerStory* self, Ctx* cx, const ClickEvent*,
                        intptr_t picker) {
    PickerState& state = self->pickers[picker];
    state.start = {};
    state.end = {};
    self->open = -1;
    UpdateValue(self, (int)picker);
    Notify(cx);
}

static void PickDate(DatePickerStory* self, Ctx* cx, const ClickEvent*,
                     intptr_t key) {
    if (self->open < 0) {
        return;
    }
    int picker = self->open;
    PickerState& state = self->pickers[picker];
    LocalDate date = DatePickerDateFromKey(key);
    DateSelectionResult result = DatePickerSelectDate(
        state.range, date, &state.start, &state.end, state.disabled);
    if (result == DateSelectionResult::Rejected) {
        return;
    }
    state.viewYear = state.start.year;
    state.viewMonth = state.start.month;
    if (result == DateSelectionResult::Complete) {
        self->open = -1;
        UpdateValue(self, picker);
    }
    Notify(cx);
}

static void PrevMonth(DatePickerStory* self, Ctx* cx, const ClickEvent*,
                      intptr_t picker) {
    PickerState& state = self->pickers[picker];
    if (state.calendarView == CalendarView::Month) {
        return;
    }
    if (state.calendarView == CalendarView::Year) {
        state.yearPageStart = std::max(state.yearMin, state.yearPageStart - 20);
        Notify(cx);
        return;
    }
    CalendarState calendar;
    calendar.currentYear = state.viewYear;
    calendar.currentMonth = state.viewMonth;
    CalendarPrevMonth(&calendar);
    state.viewYear = calendar.currentYear;
    state.viewMonth = calendar.currentMonth;
    Notify(cx);
}

static void NextMonth(DatePickerStory* self, Ctx* cx, const ClickEvent*,
                      intptr_t picker) {
    PickerState& state = self->pickers[picker];
    if (state.calendarView == CalendarView::Month) {
        return;
    }
    if (state.calendarView == CalendarView::Year) {
        if (state.yearPageStart + 20 < state.yearMax) {
            state.yearPageStart += 20;
        }
        Notify(cx);
        return;
    }
    CalendarState calendar;
    calendar.currentYear = state.viewYear;
    calendar.currentMonth = state.viewMonth;
    CalendarNextMonth(&calendar);
    state.viewYear = calendar.currentYear;
    state.viewMonth = calendar.currentMonth;
    Notify(cx);
}

static void ToggleMonthView(DatePickerStory* self, Ctx* cx, const ClickEvent*,
                            intptr_t picker) {
    PickerState& state = self->pickers[picker];
    state.calendarView = state.calendarView == CalendarView::Month
                             ? CalendarView::Day
                             : CalendarView::Month;
    Notify(cx);
}

static void ToggleYearView(DatePickerStory* self, Ctx* cx, const ClickEvent*,
                           intptr_t picker) {
    PickerState& state = self->pickers[picker];
    state.calendarView = state.calendarView == CalendarView::Year
                             ? CalendarView::Day
                             : CalendarView::Year;
    Notify(cx);
}

static void SelectMonth(DatePickerStory* self, Ctx* cx, const ClickEvent*,
                        intptr_t month) {
    if (self->open < 0) {
        return;
    }
    PickerState& state = self->pickers[self->open];
    state.viewMonth = (int)month;
    state.calendarView = CalendarView::Day;
    Notify(cx);
}

static void SelectYear(DatePickerStory* self, Ctx* cx, const ClickEvent*,
                       intptr_t year) {
    if (self->open < 0) {
        return;
    }
    PickerState& state = self->pickers[self->open];
    state.viewYear = (int)year;
    state.calendarView = CalendarView::Day;
    Notify(cx);
}

static void SelectPreset(DatePickerStory* self, Ctx* cx, const ClickEvent*,
                         intptr_t arg) {
    int picker = (int)arg / 16;
    int presetIx = (int)arg % 16;
    const component::DateRangePreset& preset = picker == DpDefault
                                                   ? gSinglePresets[presetIx]
                                                   : gRangePresets[presetIx];
    PickerState& state = self->pickers[picker];
    state.start = preset.start;
    state.end = preset.end;
    state.viewYear = preset.start.year;
    state.viewMonth = preset.start.month;
    self->focused = picker;
    self->open = -1;
    UpdateValue(self, picker);
    Notify(cx);
}

static component::DatePicker* Picker(DatePickerStory* self, Ctx* cx, int picker,
                                     Str id) {
    const PickerState& state = self->pickers[picker];
    return component::DatePicker::New(cx)
        ->Id(id)
        ->Year(state.start.year)
        ->Month(state.start.month)
        ->Day(state.start.day)
        ->RangeEnd(state.end.year, state.end.month, state.end.day)
        ->View(state.viewYear, state.viewMonth)
        ->Range(state.range)
        ->CalendarMode(state.calendarView)
        ->YearRange(state.yearMin, state.yearMax, state.yearPageStart)
        ->DisabledMatcher(state.disabled)
        ->WithSize(self->toolbar.size)
        ->W(280)
        ->Open(self->open == picker)
        ->OnToggle(ListenerArg(Listen(cx, &TogglePicker), picker))
        ->OnClear(ListenerArg(Listen(cx, &ClearPicker), picker))
        ->OnDate(Listen(cx, &PickDate))
        ->OnPrev(ListenerArg(Listen(cx, &PrevMonth), picker))
        ->OnNext(ListenerArg(Listen(cx, &NextMonth), picker))
        ->OnMonthToggle(ListenerArg(Listen(cx, &ToggleMonthView), picker))
        ->OnYearToggle(ListenerArg(Listen(cx, &ToggleYearView), picker))
        ->OnMonth(Listen(cx, &SelectMonth))
        ->OnYear(Listen(cx, &SelectYear));
}

static void InitializeStory(DatePickerStory* self) {
    if (self->seeded) {
        return;
    }
    self->seeded = true;
    LocalDate now = DateToday();
    for (int i = 0; i < DpCount; i++) {
        self->pickers[i].viewYear = now.year;
        self->pickers[i].viewMonth = now.month;
        self->pickers[i].yearMin = now.year - 50;
        self->pickers[i].yearMax = now.year + 50;
        self->pickers[i]
            .yearPageStart = self->pickers[i].yearMin +
                             ((now.year - self->pickers[i].yearMin) / 20) * 20;
    }

    self->pickers[DpDefault].start = now;
    self->pickers[DpDefault]
        .disabled = DateMatcherWeekdays((1u << 0) | (1u << 6));

    self->pickers[DpSmall].start = now;
    self->pickers[DpSmall]
        .disabled = DateMatcherInterval(now, DateAddDays(now, 5));

    self->pickers[DpLarge].start = DateAddDays(now, -1);
    self->pickers[DpLarge].viewYear = self->pickers[DpLarge].start.year;
    self->pickers[DpLarge].viewMonth = self->pickers[DpLarge].start.month;
    self->pickers[DpLarge]
        .disabled = DateMatcherRange(now, DateAddDays(now, 7));

    self->pickers[DpCustom].start = now;
    self->pickers[DpCustom].disabled = DateMatcherCustom(&FirstFiveDays);

    self->pickers[DpDateRange].range = true;
    self->pickers[DpDateRange].start = now;
    self->pickers[DpDateRange].end = DateAddDays(now, 4);
    self->pickers[DpEmptyRange].range = true;

    self->pickers[DpBirthday].yearMin = 1927;
    self->pickers[DpBirthday].yearMax = now.year + 1;
    self->pickers[DpBirthday].yearPageStart =
        self->pickers[DpBirthday].yearMin +
        ((now.year - self->pickers[DpBirthday].yearMin) / 20) * 20;

    static const char* singleLabels[] = {"Yesterday", "Last Week",
                                         "Last Month"};
    static const int singleDays[] = {1, 7, 30};
    for (int i = 0; i < 3; i++) {
        gSinglePresets[i].label = Str(singleLabels[i]);
        gSinglePresets[i].start = DateAddDays(now, -singleDays[i]);
        gSinglePresets[i].arg = DpDefault * 16 + i;
    }
    static const char* rangeLabels[] = {"Last 7 Days", "Last 14 Days",
                                        "Last 30 Days", "Last 90 Days"};
    static const int rangeDays[] = {7, 14, 30, 90};
    for (int i = 0; i < 4; i++) {
        gRangePresets[i].label = Str(rangeLabels[i]);
        gRangePresets[i].start = DateAddDays(now, -rangeDays[i]);
        gRangePresets[i].end = now;
        gRangePresets[i].arg = i;
    }
}

El* DatePickerStory::Render(DatePickerStory* self, Ctx* cx) {
    InitializeStory(self);
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    for (int i = 0; i < 4; i++) {
        gRangePresets[i].arg = DpDateRange * 16 + i;
    }

    El* page = Div(a)->FlexCol()->Gap(12)->W(kFill);
    page->Child(StoryToolbar(cx, self));

    El* defaults =
        StorySection(cx, "Default",
                     "Single-date selection with presets and clear action.")
            ->W(512);
    El* defaultContent = Div(a)->FlexCol()->Gap(12);
    defaultContent
        ->Child(Picker(self, cx, DpDefault, StrL("date-picker-default"))
                    ->Cleanable()
                    ->Presets(gSinglePresets, 3, Listen(cx, &SelectPreset))
                    ->IntoEl());
    defaultContent->Child(
        StoryTxt(cx, StoryFmt(cx, "Value: %s", self->value), 14, th.mutedFg));
    StorySectionAdd(defaults, defaultContent);
    page->Child(defaults);

    El* disabled =
        StorySection(cx, "Disabled dates",
                     "Matchers can block intervals, ranges, or custom dates.")
            ->W(512);
    El* disabledContent = Div(a)->FlexCol()->Gap(12);
    disabledContent
        ->Child(Picker(self, cx, DpSmall, StrL("date-picker-small"))->IntoEl());
    disabledContent->Child(Picker(self, cx, DpLarge, StrL("date-picker-large"))
                               ->Format(component::DateFormat::Dash)
                               ->IntoEl());
    disabledContent->Child(
        Picker(self, cx, DpCustom, StrL("date-picker-custom"))->IntoEl());
    StorySectionAdd(disabled, disabledContent);
    page->Child(disabled);

    El* range = StorySection(cx, "Date range", "Two months with range presets.")
                    ->W(512);
    for (int i = 0; i < 4; i++) {
        gRangePresets[i].arg = DpDateRange * 16 + i;
    }
    StorySectionAdd(range,
                    Picker(self, cx, DpDateRange, StrL("date-picker-range"))
                        ->NumberOfMonths(2)
                        ->Cleanable()
                        ->Presets(gRangePresets, 4, Listen(cx, &SelectPreset))
                        ->IntoEl());
    page->Child(range);

    El* empty = StorySection(cx, "Empty range", "Empty range with presets.")
                    ->W(512);
    for (int i = 0; i < 4; i++) {
        gRangePresets[i].arg = DpEmptyRange * 16 + i;
    }
    StorySectionAdd(
        empty, Picker(self, cx, DpEmptyRange, StrL("date-picker-empty-range"))
                   ->Placeholder(StrL("Range mode picker"))
                   ->Cleanable()
                   ->Presets(gRangePresets, 4, Listen(cx, &SelectPreset))
                   ->IntoEl());
    page->Child(empty);

    El* birthday = StorySection(cx, "Year range", "Custom year range.")->W(512);
    StorySectionAdd(birthday,
                    Picker(self, cx, DpBirthday, StrL("date-picker-birthday"))
                        ->Placeholder(StrL("Select birthday"))
                        ->Cleanable()
                        ->IntoEl());
    page->Child(birthday);

    El* custom = StorySection(cx, "Custom style", "Appearance-free input.")
                     ->W(512);
    StorySectionAdd(custom,
                    Div(a)
                        ->W(280)
                        ->Bg(th.secondary)
                        ->Child(Picker(self, cx, DpNoAppearance,
                                       StrL("date-picker-no-appearance"))
                                    ->Appearance(false)
                                    ->Placeholder(StrL("Without appearance"))
                                    ->IntoEl()));
    page->Child(custom);
    return page;
}

void DatePickerStory::OnKey(DatePickerStory* self, Ctx* cx,
                            const KeyEvent* ev) {
    if (!ev->down) {
        return;
    }
    DatePickerAction action =
        DatePickerActionForKey(ev->vk, self->open >= 0, false);
    if (action == DatePickerAction::Clear) {
        PickerState& state = self->pickers[self->focused];
        state.start = {};
        state.end = {};
        self->open = -1;
        UpdateValue(self, self->focused);
    } else if (action == DatePickerAction::Open) {
        self->open = self->focused;
    } else if (action == DatePickerAction::Dismiss) {
        self->open = -1;
    } else {
        return;
    }
    Notify(cx);
}

STORY_PAGE_KEYS(StoryDatePicker, DatePickerStory);
