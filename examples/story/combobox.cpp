#include "Story.h"

// The items each section lists. A SearchableList keeps a pointer to them, so
// they are static rather than built on the frame arena.
static const component::SearchableItem kFrameworks[] = {
    {StrL("GPUI"), StrL("gpui"), 0, false},
    {StrL("Iced"), StrL("iced"), 0, false},
    {StrL("egui"), StrL("egui"), 0, false},
    {StrL("Slint"), StrL("slint"), 0, false},
    {StrL("Tauri"), StrL("tauri"), 0, false},
    {StrL("Dioxus"), StrL("dioxus"), 0, false},
};
// Two groups, which is what the grouped section shows.
static const Str kFruitGroups[] = {StrL("Pome"), StrL("Citrus")};
static const component::SearchableItem kFruits[] = {
    {StrL("Apples"), StrL("apples"), 0, false},
    {StrL("Cherries"), StrL("cherries"), 0, false},
    {StrL("Oranges"), StrL("oranges"), 1, false},
    {StrL("Lemons"), StrL("lemons"), 1, false},
};
// One of them is unavailable, and stays visible.
static const component::SearchableItem kDisabledItems[] = {
    {StrL("Apples"), StrL("apples"), 0, false},
    {StrL("Bananas"), StrL("bananas"), 0, true},
    {StrL("Cherries"), StrL("cherries"), 0, false},
    {StrL("Oranges"), StrL("oranges"), 0, true},
};
static const component::SearchableItem kIndustries[] = {
    {StrL("Airlines / Aviation"), StrL("aviation"), 0, false},
    {StrL("Automotive"), StrL("automotive"), 0, false},
    {StrL("Think Tanks"), StrL("think-tanks"), 0, false},
    {StrL("Education"), StrL("education"), 0, false},
};
static const component::SearchableItem kUniversities[] = {
    {StrL("MIT"), StrL("mit"), 0, false},
    {StrL("Stanford"), StrL("stanford"), 0, false},
    {StrL("Oxford"), StrL("oxford"), 0, false},
    {StrL("Cambridge"), StrL("cambridge"), 0, false},
};

#define COMBO_COUNT(a) (int)(sizeof(a) / sizeof(a[0]))

// One combobox per section, in the order the Rust story renders them.
struct ComboSpec {
    const char* id;
    const char* title;
    const char* description;
    const char* placeholder;
    const component::SearchableItem* items;
    int count;
    const Str* groups;
    int nGroups;
    bool multiple;
    bool icon;
};

static const ComboSpec kSpecs[] = {
    {"basic", "Default", "Search and choose one option.", "Select framework...",
     kFrameworks, COMBO_COUNT(kFrameworks), nullptr, 0, false, false},
    {"basic-multi", "Multiple", "Select more than one option.",
     "Select frameworks...", kFrameworks, COMBO_COUNT(kFrameworks), nullptr, 0,
     true, false},
    {"grouped", "Groups", "Organize results into groups.", "Select item...",
     kFruits, COMBO_COUNT(kFruits), kFruitGroups, 2, false, false},
    {"disabled-items", "Disabled items", "Keep unavailable options visible.",
     "Select item...", kDisabledItems, COMBO_COUNT(kDisabledItems), nullptr, 0,
     false, false},
    {"with-icon", "Icons", "Show icons in options and the trigger.",
     "Select industry category", kIndustries, COMBO_COUNT(kIndustries), nullptr,
     0, false, true},
    {"footer", "Cleanable", "Clear the value from the trigger.",
     "Select university", kUniversities, COMBO_COUNT(kUniversities), nullptr, 0,
     false, false},
    {"count", "Count", "Summarize selections as a count.", "Select frameworks",
     kFrameworks, COMBO_COUNT(kFrameworks), nullptr, 0, true, false},
};
static const int kNSpecs = (int)(sizeof(kSpecs) / sizeof(kSpecs[0]));

struct ComboboxStory {
    // One list per combobox: the items, the query, the selection and whether
    // it is open are all its own.
    Entity<component::SearchableListState> combo[8] = {};
    InputState query;
    bool seeded = false;

    static El* Render(ComboboxStory* self, Ctx* cx);
    static void OnKey(ComboboxStory* self, Ctx* cx, const KeyEvent* ev);
};

static void ToggleCombo(ComboboxStory* self, Ctx* cx, const ClickEvent*,
                        intptr_t which) {
    for (int i = 0; i < kNSpecs; i++) {
        component::SearchableListState* s = self->combo[i].Get(cx);
        if (!s) {
            continue;
        }
        if (i == (int)which) {
            component::SelectToggleOpen(s, cx);
        } else {
            s->open = false;
        }
    }
    // The query is shared, so it starts empty every time one opens.
    InputSetValue(&self->query, Str{});
    self->query.focused = true;
    Notify(cx);
}
static void ClearCombo(ComboboxStory* self, Ctx* cx, const ClickEvent*,
                       intptr_t which) {
    component::SelectClear(self->combo[which].Get(cx), cx);
}
static void FocusQuery(ComboboxStory* self, Ctx* cx, const ClickEvent*) {
    self->query.focused = true;
    Notify(cx);
}

static component::SearchableListState* OpenCombo(ComboboxStory* self, Ctx* cx) {
    for (int i = 0; i < kNSpecs; i++) {
        component::SearchableListState* s = self->combo[i].Get(cx);
        if (s && s->open) {
            return s;
        }
    }
    return nullptr;
}

El* ComboboxStory::Render(ComboboxStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    if (!self->seeded) {
        self->seeded = true;
        InputSetPlaceholder(&self->query, StrL("Search…"));
        for (int i = 0; i < kNSpecs; i++) {
            self->combo[i] =
                EntityNewState<component::SearchableListState>(cx->app);
        }
        // The grouped section opens with a value picked, as the Rust story
        // does.
        component::SearchableListState* grouped = self->combo[2].Get(cx);
        if (grouped) {
            grouped->selected[0] = 0;
            grouped->nSelected = 1;
        }
    }
    if (self->query.focused) {
        cx->win->input = &self->query;
    }
    Listener toggle = Listen(cx, &ToggleCombo);
    Listener clear = Listen(cx, &ClearCombo);

    El* page = Div(a)->FlexCol()->Gap(16)->W(kFill)->ItemsCenter();
    for (int i = 0; i < kNSpecs; i++) {
        const ComboSpec& s = kSpecs[i];
        El* sec = StorySection(cx, s.title, s.description);
        component::Combobox* cb =
            component::Combobox::New(cx, Str(s.id), self->combo[i],
                                     &self->query)
                ->Items(s.items, s.count)
                ->Placeholder(Str(s.placeholder))
                ->SearchPlaceholder(StrL("Search…"))
                ->W(280)
                ->OnQueryFocus(Listen(cx, &FocusQuery))
                ->OnToggle(ListenerArg(toggle, (intptr_t)i))
                ->OnClear(ListenerArg(clear, (intptr_t)i));
        if (s.groups) {
            cb->Sections(s.groups, s.nGroups);
        }
        if (s.multiple) {
            cb->Multiple();
        }
        if (s.icon) {
            cb->Icon(IconName::Building2);
        }
        if (i == 5) {
            cb->Cleanable();
        }
        StorySectionAdd(sec, cb->IntoEl());
        page->Child(sec);
    }

    // The last section reads back what each list holds.
    El* values =
        StorySection(cx, "Values", "Read selected values from each list.");
    El* valueCol = Div(a)->FlexCol()->W(280)->Gap(8);
    for (int i = 0; i < 3; i++) {
        component::SearchableListState* s = self->combo[i].Get(cx);
        Str line = StoryFmt(cx, "%s: None", kSpecs[i].id);
        if (s && s->nSelected == 1) {
            line = StoryFmt(cx, "%s: Some(\"%s\")", kSpecs[i].id,
                            kSpecs[i].items[s->selected[0]].title);
        } else if (s && s->nSelected > 1) {
            line = StoryFmt(cx, "%s: %d selected", kSpecs[i].id, s->nSelected);
        }
        valueCol->Child(StoryTxt(cx, line, 16, th.foreground));
    }
    StorySectionAdd(values, valueCol);
    page->Child(values);
    return page;
}

// A combobox is a select in Rust — Combobox::render builds one and forwards
// everything to it — so it answers to the same four keys.
void ComboboxStory::OnKey(ComboboxStory* self, Ctx* cx, const KeyEvent* ev) {
    if (!ev->down) {
        return;
    }
    component::SearchableListState* s = OpenCombo(self, cx);
    SelectAction act = SelectActionForKey(ev->vk, s != nullptr, false);
    if (!s) {
        return;
    }
    if (act == SelectAction::Dismiss) {
        s->open = false;
        s->list.selected = -1;
        Notify(cx);
        return;
    }
    if (act == SelectAction::Confirm) {
        // Rust's combobox confirms without closing when it is multiple: the
        // list decides, since close_on_select is its own.
        if (s->list.selected >= 0 && s->list.selected < s->nMatches) {
            component::SearchableListClick(s, s->matches[s->list.selected]);
        }
        cx->win->eatReturn = true;
        Notify(cx);
        return;
    }
    if (ev->vk == KeyUp || ev->vk == KeyDown) {
        ListPerform(
            &s->list, cx,
            ev->vk == KeyDown ? ListAction::SelectNext : ListAction::SelectPrev,
            false);
    }
}

STORY_PAGE_KEYS(StoryCombobox, ComboboxStory);
