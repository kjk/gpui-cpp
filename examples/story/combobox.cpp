#include "Story.h"

// The items each section lists, straight out of combobox_story.rs. A
// SearchableList keeps a pointer to them, so they are static rather than
// built on the frame arena.
static const component::SearchableItem kFrameworks[] = {
    {StrL("Next.js"), StrL("Next.js"), 0, false, IconName::None},
    {StrL("SvelteKit"), StrL("SvelteKit"), 0, false, IconName::None},
    {StrL("Nuxt.js"), StrL("Nuxt.js"), 0, false, IconName::None},
    {StrL("Remix"), StrL("Remix"), 0, false, IconName::None},
    {StrL("Astro"), StrL("Astro"), 0, false, IconName::None},
};
static const component::SearchableItem kMultiFrameworks[] = {
    {StrL("React"), StrL("React"), 0, false, IconName::None},
    {StrL("Nextjs"), StrL("Nextjs"), 0, false, IconName::None},
    {StrL("Angular"), StrL("Angular"), 0, false, IconName::None},
    {StrL("VueJS"), StrL("VueJS"), 0, false, IconName::None},
    {StrL("Django"), StrL("Django"), 0, false, IconName::None},
    {StrL("Astro"), StrL("Astro"), 0, false, IconName::None},
    {StrL("Remix"), StrL("Remix"), 0, false, IconName::None},
    {StrL("Svelte"), StrL("Svelte"), 0, false, IconName::None},
    {StrL("SolidJS"), StrL("SolidJS"), 0, false, IconName::None},
    {StrL("Qwik"), StrL("Qwik"), 0, false, IconName::None},
};
// food_groups(): three groups, with one item disabled in two of them.
static const Str kFoodGroups[] = {StrL("Fruits"), StrL("Vegetables"),
                                  StrL("Beverages")};
static const component::SearchableItem kFoods[] = {
    {StrL("Apples"), StrL("Apples"), 0, false, IconName::None},
    {StrL("Bananas"), StrL("Bananas"), 0, false, IconName::None},
    {StrL("Cherries"), StrL("Cherries"), 0, false, IconName::None},
    {StrL("Carrots"), StrL("Carrots"), 1, false, IconName::None},
    {StrL("Broccoli"), StrL("Broccoli"), 1, true, IconName::None},
    {StrL("Spinach"), StrL("Spinach"), 1, false, IconName::None},
    {StrL("Tea"), StrL("Tea"), 2, false, IconName::None},
    {StrL("Coffee"), StrL("Coffee"), 2, true, IconName::None},
    {StrL("Juice"), StrL("Juice"), 2, false, IconName::None},
};
static const component::SearchableItem kDisabledItems[] = {
    {StrL("Apples"), StrL("Apples"), 0, false, IconName::None},
    {StrL("Bananas"), StrL("Bananas"), 0, true, IconName::None},
    {StrL("Cherries"), StrL("Cherries"), 0, false, IconName::None},
    {StrL("Carrots"), StrL("Carrots"), 0, false, IconName::None},
    {StrL("Broccoli"), StrL("Broccoli"), 0, true, IconName::None},
};
// industries(): each row draws its own icon.
static const component::SearchableItem kIndustries[] = {
    {StrL("Information Technology"), StrL("Information Technology"), 0, false,
     IconName::Cpu},
    {StrL("Healthcare"), StrL("Healthcare"), 0, false, IconName::Heart},
    {StrL("Finance"), StrL("Finance"), 0, false, IconName::Globe},
    {StrL("Education"), StrL("Education"), 0, false, IconName::BookOpen},
    {StrL("Entertainment"), StrL("Entertainment"), 0, false, IconName::Star},
};
static const component::SearchableItem kUniversities[] = {
    {StrL("Harvard University"), StrL("Harvard University"), 0, false,
     IconName::None},
    {StrL("MIT"), StrL("MIT"), 0, false, IconName::None},
    {StrL("Stanford"), StrL("Stanford"), 0, false, IconName::None},
    {StrL("Cambridge"), StrL("Cambridge"), 0, false, IconName::None},
};

#define COMBO_COUNT(a) (int)(sizeof(a) / sizeof(a[0]))

// One combobox per section, in the order the Rust story renders them. The
// four it also has — Custom trigger, Badges, Maximum selections, Pinned
// items, Rich items and Overflow — all hang off `render_trigger`,
// `is_item_checked`, `render_item` and `on_will_change`, delegate hooks the
// port does not have a surface for yet.
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
    // Which items start selected, as a bit per index.
    unsigned selected;
    // check_icon(Icon::new(IconName::CircleCheck)).
    IconName checkIcon;
};

static const ComboSpec kSpecs[] = {
    {"basic", "Default", "Search and choose one option.", "Select framework...",
     kFrameworks, COMBO_COUNT(kFrameworks), nullptr, 0, false, 0,
     IconName::Check},
    {"basic-multi", "Multiple", "Select more than one option.",
     "Select frameworks...", kFrameworks, COMBO_COUNT(kFrameworks), nullptr, 0,
     true, 0, IconName::Check},
    {"grouped", "Groups", "Organize results into groups.", "Select item...",
     kFoods, COMBO_COUNT(kFoods), kFoodGroups, 3, false, 1u << 0,
     IconName::Check},
    {"disabled-items", "Disabled items", "Keep unavailable options visible.",
     "Select item...", kDisabledItems, COMBO_COUNT(kDisabledItems), nullptr, 0,
     false, 0, IconName::Check},
    {"with-icon", "Icons", "Show icons in options and the trigger.",
     "Select industry category", kIndustries, COMBO_COUNT(kIndustries), nullptr,
     0, false, 0, IconName::Check},
    {"custom-check", "Check icon", "Replace the default selection mark.",
     "Select framework...", kFrameworks, COMBO_COUNT(kFrameworks), nullptr, 0,
     false, 0, IconName::CircleCheck},
    {"with-footer", "Footer", "Add an action below the option list.",
     "Select university", kUniversities, COMBO_COUNT(kUniversities), nullptr, 0,
     false, 1u << 0, IconName::Check},
    {"multi-count", "Count", "Summarize selections as a count.",
     "Select frameworks", kMultiFrameworks, COMBO_COUNT(kMultiFrameworks),
     nullptr, 0, true, 0x3f, IconName::Check},
};
static const int kNSpecs = (int)(sizeof(kSpecs) / sizeof(kSpecs[0]));

struct ComboboxStory {
    // One list per combobox: the items, the query, the selection and whether
    // it is open are all its own.
    Entity<component::SearchableListState> combo[kNSpecs] = {};
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
            component::SearchableListState* s = self->combo[i].Get(cx);
            if (!s) {
                continue;
            }
            for (int k = 0; k < kSpecs[i].count; k++) {
                if (kSpecs[i].selected & (1u << k)) {
                    s->selected[s->nSelected++] = k;
                }
            }
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
        StorySectionBody(sec)->W(280);
        component::Combobox* cb =
            component::Combobox::New(cx, Str(s.id), self->combo[i],
                                     &self->query)
                ->Items(s.items, s.count)
                ->Placeholder(Str(s.placeholder))
                ->SearchPlaceholder(StrL("Search…"))
                ->CheckIcon(s.checkIcon)
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
        // Rust's Icons section draws the *selected* item's icon before the
        // label and keeps its caret; Combobox::Icon here replaces the caret,
        // which is not the same thing, so the trigger is left alone and only
        // the rows carry icons.
        StorySectionAdd(sec, cb->IntoEl());
        page->Child(sec);
    }

    // The last section reads back what each list holds.
    El* values =
        StorySection(cx, "Values", "Read selected values from each delegate.");
    El* valueCol = Div(a)->FlexCol()->W(kFill)->Gap(8);
    static const int kShown[] = {0, 2, 7};
    for (int k = 0; k < 3; k++) {
        int i = kShown[k];
        component::SearchableListState* s = self->combo[i].Get(cx);
        Str line = StoryFmt(cx, "%s: []", kSpecs[i].id);
        if (s && s->nSelected == 1) {
            line = StoryFmt(cx, "%s: [\"%s\"]", kSpecs[i].id,
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
