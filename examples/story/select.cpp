#include "Story.h"

// One entry per select on the page.
enum {
    SelCountry = 0,
    SelFruit,
    SelDisabled,
    SelUi1,
    SelMenuH,
    SelLanguage,
    SelEmpty,
    SelAppearance,
    SelCount
};

static const char* kCountries[] = {
    "Afghanistan (AF)", "Albania (AL)",   "Algeria (DZ)",
    "Andorra (AD)",     "Angola (AO)",    "Argentina (AR)",
    "Armenia (AM)",     "Australia (AU)", "Austria (AT)",
};
static const char* kFruits[] = {
    "Apple",
    "Orange",
    "Banana",
    "Grape",
    "Pineapple",
    "Watermelon & This is a long long long long long long long long long title",
    "Avocado",
};
static const char* kUi[] = {"GPUI", "Iced",  "egui",  "Makepad", "Slint",
                            "QT",   "ImGui", "Cocoa", "WinUI"};
static const char* kLanguages[] = {"Rust", "Go", "C++", "JavaScript"};
static const char* kCodes[] = {"CN", "US", "HK", "JP", "KR"};

// The items each select shows. A SearchableList keeps a pointer to them, so
// they have to outlive the frame — which is what makes these static rather
// than built on the frame arena.
static component::SearchableItem gItems[SelCount][16];
static int gCounts[SelCount];

static void BuildItems(int which, const char* const* names, int n) {
    if (n > 16) {
        n = 16;
    }
    for (int i = 0; i < n; i++) {
        gItems[which][i].title = Str(names[i]);
        // The title is the value here: nothing on this page has an id of its
        // own behind what it shows.
        gItems[which][i].value = Str(names[i]);
    }
    gCounts[which] = n;
}

struct SelectStory {
    // SelectState is an entity in Rust too — it is the SearchableList under
    // the trigger, and it holds the selection, the query and whether the list
    // is open.
    Entity<component::SearchableListState> sel[SelCount] = {};
    bool disabled = false;
    InputState phone;
    InputState search;
    StoryToolbarState toolbar;
    bool seeded = false;

    static El* Render(SelectStory* self, Ctx* cx);
};

enum {
    SelOptDisabled = ToolbarOptDisabled
};

static void SelToolbarAct(SelectStory* self, Ctx* cx, const ClickEvent*,
                          intptr_t act) {
    if (act == SelOptDisabled) {
        self->disabled = !self->disabled;
    } else {
        StoryToolbarApply(&self->toolbar, nullptr, (int)act);
    }
    Notify(cx);
}

// Only one select is open at a time, which is what closing the rest does.
static void ToggleSel(SelectStory* self, Ctx* cx, const ClickEvent*,
                      intptr_t which) {
    for (int i = 0; i < SelCount; i++) {
        component::SearchableListState* s = self->sel[i].Get(cx);
        if (!s) {
            continue;
        }
        if (i == (int)which) {
            component::SelectToggleOpen(s, cx);
        } else {
            s->open = false;
        }
    }
    Notify(cx);
}
static void ClearSel(SelectStory* self, Ctx* cx, const ClickEvent*,
                     intptr_t which) {
    component::SelectClear(self->sel[which].Get(cx), cx);
}
static void FocusPhone(SelectStory* self, Ctx* cx, const ClickEvent*) {
    self->phone.focused = true;
    self->search.focused = false;
    Notify(cx);
}
static void FocusSearch(SelectStory* self, Ctx* cx, const ClickEvent*) {
    self->search.focused = true;
    self->phone.focused = false;
    Notify(cx);
}

static component::Select* Sel(SelectStory* self, Ctx* cx, int which,
                              const char* id, Listener toggle, Listener clear) {
    return component::Select::New(cx, Str(id), self->sel[which])
        ->Items(gItems[which], gCounts[which])
        ->W(280)
        ->WithSize(self->toolbar.size)
        ->Disabled(self->disabled)
        ->OnToggle(ListenerArg(toggle, which))
        ->OnClear(ListenerArg(clear, which));
}

El* SelectStory::Render(SelectStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    if (!self->seeded) {
        self->seeded = true;
        InputSetPlaceholder(&self->phone, StrL("Your phone number"));
        InputSetPlaceholder(&self->search, StrL("Search..."));
        for (int i = 0; i < SelCount; i++) {
            self->sel[i] =
                EntityNewState<component::SearchableListState>(cx->app);
        }
        BuildItems(SelCountry, kCountries,
                   (int)(sizeof(kCountries) / sizeof(char*)));
        BuildItems(SelFruit, kFruits, (int)(sizeof(kFruits) / sizeof(char*)));
        BuildItems(SelUi1, kUi, (int)(sizeof(kUi) / sizeof(char*)));
        BuildItems(SelMenuH, kUi, (int)(sizeof(kUi) / sizeof(char*)));
        BuildItems(SelLanguage, kLanguages,
                   (int)(sizeof(kLanguages) / sizeof(char*)));
        BuildItems(SelAppearance, kCodes,
                   (int)(sizeof(kCodes) / sizeof(char*)));
        // The three selects Rust seeds with `Some(IndexPath::default())`
        // open with a value already picked: the country, and the two that
        // carry a title prefix.
        component::SearchableListState* country = self->sel[SelCountry].Get(cx);
        if (country) {
            country->selected[0] = 5;
            country->selected.len = 1;
        }
        for (int which : {SelUi1, SelMenuH}) {
            component::SearchableListState* st = self->sel[which].Get(cx);
            if (st) {
                st->selected[0] = 0;
                st->selected.len = 1;
            }
        }
    }
    if (self->phone.focused) {
        cx->win->input = &self->phone;
    } else if (self->search.focused) {
        cx->win->input = &self->search;
    }
    Listener toggle = Listen(cx, &ToggleSel);
    Listener clear = Listen(cx, &ClearSel);

    El* page = Div(a)->FlexCol()->Gap(16)->W(kFill)->ItemsCenter();
    StoryToolbarOpt opts[1] = {{"Disabled", self->disabled, SelOptDisabled}};
    page->Child(
        StoryToolbarOptions(cx, self, opts, 1, Listen(cx, &SelToolbarAct)));

    El* search = StorySection(cx, "Search and clear",
                              "Search options and clear the value.");
    StorySectionAdd(search,
                    Sel(self, cx, SelCountry, "country", toggle, clear)
                        ->Cleanable()
                        ->Searchable(&self->search, Listen(cx, &FocusSearch))
                        ->IntoEl());
    page->Child(search);

    El* width = StorySection(cx, "Menu width",
                             "Set trigger and menu widths independently.");
    StorySectionAdd(width, Sel(self, cx, SelFruit, "fruit", toggle, clear)
                               ->Icon(IconName::Search)
                               ->MenuWidth(400)
                               ->IntoEl());
    page->Child(width);

    El* dis = StorySection(cx, "Disabled", "Keep the selected value visible.");
    StorySectionAdd(dis, component::Select::New(cx, StrL("select-disabled"),
                                                self->sel[SelDisabled])
                             ->W(280)
                             ->WithSize(self->toolbar.size)
                             ->Disabled(true)
                             ->IntoEl());
    page->Child(dis);

    El* prefix = StorySection(cx, "Title prefix", "Prefix the selected value.");
    StorySectionAdd(prefix, Sel(self, cx, SelUi1, "ui1", toggle, clear)
                                ->Placeholder(StrL("UI"))
                                ->TitlePrefix(StrL("UI: "))
                                ->IntoEl());
    page->Child(prefix);

    El* menuH = StorySection(cx, "Menu height", "Limit the popup height.");
    StorySectionAdd(menuH, Sel(self, cx, SelMenuH, "menu-h", toggle, clear)
                               ->Placeholder(StrL("UI"))
                               ->TitlePrefix(StrL("UI: "))
                               ->MenuMaxH(96)
                               ->IntoEl());
    page->Child(menuH);

    El* multi = StorySection(cx, "Multiple",
                             "Pick more than one; the trigger says how many.");
    StorySectionAdd(multi, Sel(self, cx, SelLanguage, "language", toggle, clear)
                               ->Placeholder(StrL("Language"))
                               ->Multiple()
                               ->IntoEl());
    page->Child(multi);

    El* empty = StorySection(cx, "Empty", "Render a custom empty state.");
    StorySectionAdd(empty, component::Select::New(cx, StrL("select-empty"),
                                                  self->sel[SelEmpty])
                               ->W(280)
                               ->WithSize(self->toolbar.size)
                               ->Disabled(self->disabled)
                               ->Empty(StrL("No Data"))
                               ->OnToggle(ListenerArg(toggle, SelEmpty))
                               ->IntoEl());
    page->Child(empty);

    El* custom =
        StorySection(cx, "Custom appearance",
                     "Compose an appearance-free select with another control.");
    // A country code, a divider, a phone field and the send button, all in
    // one bordered row.
    El* row = Div(a)
                  ->FlexRow()
                  ->W(280)
                  ->Gap(4)
                  ->ItemsCenter()
                  ->Radius(th.radiusLg)
                  ->Border(1, th.inputBorder);
    row->Child(Div(a)->W(140)->Child(
        component::Select::New(cx, StrL("appearance"), self->sel[SelAppearance])
            ->Items(gItems[SelAppearance], gCounts[SelAppearance])
            ->W(140)
            ->WithSize(self->toolbar.size)
            ->Appearance(false)
            ->OnToggle(ListenerArg(toggle, SelAppearance))
            ->IntoEl()));
    row->Child(component::Separator::Vertical(cx)->IntoEl()->H(20));
    row->Child(Div(a)->Grow()->Child(
        component::Input::New(cx, StrL("phone"), &self->phone)
            ->Appearance(false)
            ->WithSize(self->toolbar.size)
            ->OnFocus(Listen(cx, &FocusPhone))
            ->IntoEl()));
    row->Child(Div(a)->Pad(8)->Child(component::Button::New(cx, StrL("send"))
                                         ->Ghost()
                                         ->WithSize(self->toolbar.size)
                                         ->Label(StrL("Send"))
                                         ->IntoEl()));
    StorySectionAdd(custom, row);
    page->Child(custom);

    El* values = StorySection(cx, "Values", "Read selected values from state.");
    El* valueCol = Div(a)->FlexCol()->W(512)->Gap(12);
    const char* labels[] = {"Country", "fruit", "UI", "Language"};
    int slots[] = {SelCountry, SelFruit, SelUi1, SelLanguage};
    for (int i = 0; i < 4; i++) {
        component::SearchableListState* s = self->sel[slots[i]].Get(cx);
        Str line = StoryFmt(cx, "%s: None", labels[i]);
        if (s && s->selected.len > 0) {
            line = StoryFmt(cx, "%s: Some(\"%s\")", labels[i],
                            gItems[slots[i]][s->selected[0]].title);
        }
        valueCol->Child(StoryTxt(cx, line, 16, th.foreground));
    }
    valueCol
        ->Child(StoryTxt(cx, StrL("This is other text."), 16, th.foreground));
    StorySectionAdd(values, valueCol);
    page->Child(values);
    return page;
}

STORY_PAGE(StorySelect, SelectStory);
