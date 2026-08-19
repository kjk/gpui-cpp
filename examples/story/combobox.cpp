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
    InputState query;
    bool seeded = false;

    static El* Render(ComboboxStory* self, Ctx* cx);
    static void OnKey(ComboboxStory* self, Ctx* cx, const KeyEvent* ev);
};

static void ToggleCombo(ComboboxStory* self, Ctx* cx, const ClickEvent*,
                        intptr_t which) {
    self->open = self->open == (int)which ? -1 : (int)which;
    Notify(cx);
}

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

// Esc closes what this page has open, like an overlay dismiss.
void ComboboxStory::OnKey(ComboboxStory* self, Ctx* cx, const KeyEvent* ev) {
    if (ev->vk != KeyEscape) {
        return;
    }
    self->open = -1;
    Notify(cx);
}

STORY_PAGE_KEYS(StoryCombobox, ComboboxStory);
