#include "Story.h"

static const char* kFrameworks[] = {"GPUI",  "Iced",  "egui",
                                    "Slint", "Tauri", "Dioxus"};
static const char* kFruits[] = {"Apples", "Bananas", "Cherries", "Oranges"};
static const char* kIndustries[] = {"Airlines / Aviation", "Automotive",
                                    "Think Tanks", "Education"};
static const char* kUniversities[] = {"MIT", "Stanford", "Oxford", "Cambridge"};

// One combobox per section, in the order the Rust story renders them.
struct ComboSpec {
    const char* id;
    const char* title;
    const char* description;
    const char* placeholder;
    const char* const* items;
    int count;
    const char* selected; // nullptr keeps the placeholder
    bool icon;
};

#define COMBO_COUNT(a) (int)(sizeof(a) / sizeof(char*))

struct ComboboxStory {
    int open = -1;
    // The option the arrows are on inside the open combobox.
    int highlight = -1;
    InputState query;
    bool seeded = false;

    static El* Render(ComboboxStory* self, Ctx* cx);
    static void OnKey(ComboboxStory* self, Ctx* cx, const KeyEvent* ev);
};

static void ToggleCombo(ComboboxStory* self, Ctx* cx, const ClickEvent*,
                        intptr_t which) {
    self->open = self->open == (int)which ? -1 : (int)which;
    self->highlight = -1;
    Notify(cx);
}

// How many options the combobox in this slot lists. Every section on this page
// draws from one of four arrays.
static int ComboOptionCount(int which);

El* ComboboxStory::Render(ComboboxStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    if (!self->seeded) {
        self->seeded = true;
        InputSetPlaceholder(&self->query, StrL("Search…"));
    }
    if (self->query.focused) {
        cx->win->input = &self->query;
    }
    Listener toggle = Listen(cx, &ToggleCombo);

    const ComboSpec specs[] = {
        {"basic", "Default", "Search and choose one option.",
         "Select framework...", kFrameworks, COMBO_COUNT(kFrameworks), nullptr,
         false},
        {"basic-multi", "Multiple", "Select more than one option.",
         "Select frameworks...", kFrameworks, COMBO_COUNT(kFrameworks), nullptr,
         false},
        {"grouped", "Groups", "Organize results into groups.", "Select item...",
         kFruits, COMBO_COUNT(kFruits), "Apples", false},
        {"disabled-items", "Disabled items",
         "Keep unavailable options "
         "visible.",
         "Select item...", kFruits, COMBO_COUNT(kFruits), nullptr, false},
        {"with-icon", "Icons", "Show icons in options and the trigger.",
         "Select industry category", kIndustries, COMBO_COUNT(kIndustries),
         nullptr, true},
        {"check-icon", "Check icon", "Replace the default selection mark.",
         "Select framework...", kFrameworks, COMBO_COUNT(kFrameworks), nullptr,
         false},
        {"footer", "Footer", "Add an action below the option list.",
         "Select university", kUniversities, COMBO_COUNT(kUniversities),
         nullptr, false},
        {"custom-trigger", "Custom trigger", "Render custom trigger content.",
         "Select framework", kFrameworks, COMBO_COUNT(kFrameworks), nullptr,
         false},
        {"badges", "Badges", "Show removable selected badges.",
         "Select frameworks", kFrameworks, COMBO_COUNT(kFrameworks), nullptr,
         false},
        {"max-selections", "Maximum selections",
         "Limit how many items can be selected.", "Select up to 2 frameworks",
         kFrameworks, COMBO_COUNT(kFrameworks), nullptr, false},
        {"pinned", "Pinned items", "Keep required items selected.",
         "Select framework...", kFrameworks, COMBO_COUNT(kFrameworks), nullptr,
         false},
        {"rich", "Rich items", "Render supporting content in option rows.",
         "Select framework...", kFrameworks, COMBO_COUNT(kFrameworks), nullptr,
         false},
        {"overflow", "Overflow", "Collapse selections after a visible limit.",
         "Select frameworks", kFrameworks, COMBO_COUNT(kFrameworks), nullptr,
         false},
        {"count", "Count", "Summarize selections as a count.",
         "Select frameworks", kFrameworks, COMBO_COUNT(kFrameworks), nullptr,
         false},
    };

    El* page = Div(a)->FlexCol()->Gap(16)->W(kFill)->ItemsCenter();
    for (size_t i = 0; i < sizeof(specs) / sizeof(specs[0]); i++) {
        const ComboSpec& s = specs[i];
        El* sec = StorySection(cx, s.title, s.description);
        component::Combobox* cb =
            component::Combobox::New(cx, Str(s.id))
                ->Options(s.items, s.count)
                ->Placeholder(Str(s.placeholder))
                ->SearchPlaceholder(StrL("Search…"))
                ->W(280)
                ->Open(self->open == (int)i)
                ->Highlight(self->open == (int)i ? self->highlight : -1)
                ->Query(&self->query)
                ->OnToggle(ListenerArg(toggle, (intptr_t)i));
        if (s.selected) {
            cb->Selected(Str(s.selected));
        }
        if (s.icon) {
            cb->Icon(IconName::Building2);
        }
        StorySectionAdd(sec, cb->IntoEl());
        page->Child(sec);
    }

    // The last section reads back what each delegate holds.
    El* values =
        StorySection(cx, "Values", "Read selected values from each delegate.");
    El* valueCol = Div(a)->FlexCol()->W(280)->Gap(8);
    valueCol->Child(StoryTxt(cx, StrL("basic: None"), 16, th.foreground));
    valueCol->Child(StoryTxt(cx, StrL("multi: []"), 16, th.foreground));
    valueCol->Child(
        StoryTxt(cx, StrL("grouped: Some(\"Apples\")"), 16, th.foreground));
    StorySectionAdd(values, valueCol);
    page->Child(values);
    return page;
}

// Every section here lists one of four arrays; the spec table above is built
// inside Render, so the count is recovered from the same arrays.
static int ComboOptionCount(int which) {
    switch (which) {
        case 2:
            return COMBO_COUNT(kFruits);
        case 3:
            return COMBO_COUNT(kIndustries);
        case 4:
            return COMBO_COUNT(kUniversities);
        default:
            return COMBO_COUNT(kFrameworks);
    }
}

// A combobox is a select in Rust — Combobox::render builds one and forwards
// everything to it — so it answers to the same four keys.
void ComboboxStory::OnKey(ComboboxStory* self, Ctx* cx, const KeyEvent* ev) {
    if (!ev->down) {
        return;
    }
    bool open = self->open >= 0;
    SelectAction act = SelectActionForKey(ev->vk, open, false);
    if (act == SelectAction::Dismiss) {
        self->open = -1;
        self->highlight = -1;
        Notify(cx);
        return;
    }
    if (act == SelectAction::Confirm) {
        // Rust's combobox confirms without closing: the caller decides what a
        // confirmed value does, and a multi-select stays open.
        cx->win->eatReturn = true;
        Notify(cx);
        return;
    }
    if (!open || (ev->vk != KeyUp && ev->vk != KeyDown)) {
        return;
    }
    int count = ComboOptionCount(self->open);
    int step = ev->vk == KeyDown ? 1 : -1;
    int next = self->highlight < 0 ? (step > 0 ? 0 : count - 1)
                                   : self->highlight + step;
    if (next < 0) {
        next = count - 1;
    } else if (next >= count) {
        next = 0;
    }
    self->highlight = next;
    Notify(cx);
}

STORY_PAGE_KEYS(StoryCombobox, ComboboxStory);
