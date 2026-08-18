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

struct SelectStory {
    int selected[SelCount] = {5, -1, -1, 0, 0, -1, -1, 0};
    int open = -1;
    bool disabled = false;
    LineInput phone = {};
    StoryToolbarState toolbar;
    bool seeded = false;

    static El* Render(SelectStory* self, Ctx* cx);
    static void OnKey(SelectStory* self, Ctx* cx, const KeyEvent* ev);
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
static void ToggleSel(SelectStory* self, Ctx* cx, const ClickEvent*,
                      intptr_t which) {
    self->open = self->open == (int)which ? -1 : (int)which;
    Notify(cx);
}
static void PickSel(SelectStory* self, Ctx* cx, const ClickEvent*,
                    intptr_t index) {
    if (self->open >= 0) {
        self->selected[self->open] = (int)index;
        self->open = -1;
    }
    Notify(cx);
}
static void ClearSel(SelectStory* self, Ctx* cx, const ClickEvent*,
                     intptr_t which) {
    self->selected[which] = -1;
    Notify(cx);
}
static void FocusPhone(SelectStory* self, Ctx* cx, const ClickEvent*) {
    self->phone.focused = true;
    Notify(cx);
}

static component::Select* Sel(SelectStory* self, Ctx* cx, int which,
                              const char* id, const char* const* items,
                              int count, Listener toggle, Listener pick,
                              Listener clear) {
    return component::Select::New(cx, Str(id))
        ->Options(items, count)
        ->Selected(self->selected[which])
        ->W(280)
        ->WithSize(self->toolbar.size)
        ->Disabled(self->disabled)
        ->Open(self->open == which)
        ->OnToggle(ListenerArg(toggle, which))
        ->OnChange(pick)
        ->OnClear(ListenerArg(clear, which));
}

El* SelectStory::Render(SelectStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    if (!self->seeded) {
        self->seeded = true;
        strncpy_s(self->phone.placeholder, "Your phone number", _TRUNCATE);
    }
    if (self->phone.focused) {
        cx->win->input = &self->phone;
    }
    Listener toggle = Listen(cx, &ToggleSel);
    Listener pick = Listen(cx, &PickSel);
    Listener clear = Listen(cx, &ClearSel);

    El* page = Div(a)->FlexCol()->Gap(16)->W(kFill)->ItemsCenter();
    StoryToolbarOpt opts[1] = {{"Disabled", self->disabled, SelOptDisabled}};
    page->Child(
        StoryToolbarOptions(cx, self, opts, 1, Listen(cx, &SelToolbarAct)));

    El* search = StorySection(cx, "Search and clear",
                              "Search options and clear the value.");
    StorySectionAdd(search, Sel(self, cx, SelCountry, "country", kCountries,
                                (int)(sizeof(kCountries) / sizeof(char*)),
                                toggle, pick, clear)
                                ->Cleanable()
                                ->IntoEl());
    page->Child(search);

    El* width = StorySection(cx, "Menu width",
                             "Set trigger and menu widths independently.");
    StorySectionAdd(
        width, Sel(self, cx, SelFruit, "fruit", kFruits,
                   (int)(sizeof(kFruits) / sizeof(char*)), toggle, pick, clear)
                   ->Icon(IconName::Search)
                   ->MenuWidth(400)
                   ->IntoEl());
    page->Child(width);

    El* dis = StorySection(cx, "Disabled", "Keep the selected value visible.");
    StorySectionAdd(dis, component::Select::New(cx, StrL("select-disabled"))
                             ->W(280)
                             ->WithSize(self->toolbar.size)
                             ->Disabled(true)
                             ->IntoEl());
    page->Child(dis);

    El* prefix = StorySection(cx, "Title prefix", "Prefix the selected value.");
    StorySectionAdd(prefix,
                    Sel(self, cx, SelUi1, "ui1", kUi,
                        (int)(sizeof(kUi) / sizeof(char*)), toggle, pick, clear)
                        ->Placeholder(StrL("UI"))
                        ->TitlePrefix(StrL("UI: "))
                        ->IntoEl());
    page->Child(prefix);

    El* menuH = StorySection(cx, "Menu height", "Limit the popup height.");
    StorySectionAdd(menuH,
                    Sel(self, cx, SelMenuH, "menu-h", kUi,
                        (int)(sizeof(kUi) / sizeof(char*)), toggle, pick, clear)
                        ->Placeholder(StrL("UI"))
                        ->TitlePrefix(StrL("UI: "))
                        ->MenuMaxH(96)
                        ->IntoEl());
    page->Child(menuH);

    El* searchSec =
        StorySection(cx, "Search", "Filter options from the popup.");
    StorySectionAdd(
        searchSec,
        Sel(self, cx, SelLanguage, "language", kLanguages,
            (int)(sizeof(kLanguages) / sizeof(char*)), toggle, pick, clear)
            ->Placeholder(StrL("Language"))
            ->TitlePrefix(StrL("Language: "))
            ->IntoEl());
    page->Child(searchSec);

    El* empty = StorySection(cx, "Empty", "Render a custom empty state.");
    StorySectionAdd(empty, component::Select::New(cx, StrL("select-empty"))
                               ->W(280)
                               ->WithSize(self->toolbar.size)
                               ->Disabled(self->disabled)
                               ->Empty(StrL("No Data"))
                               ->Open(self->open == SelEmpty)
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
        component::Select::New(cx, StrL("appearance"))
            ->Options(kCodes, (int)(sizeof(kCodes) / sizeof(char*)))
            ->Selected(self->selected[SelAppearance])
            ->W(140)
            ->WithSize(self->toolbar.size)
            ->Appearance(false)
            ->Open(self->open == SelAppearance)
            ->OnToggle(ListenerArg(toggle, SelAppearance))
            ->OnChange(pick)
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
    const char* const* lists[] = {kCountries, kFruits, kUi, kLanguages};
    for (int i = 0; i < 4; i++) {
        int sel = self->selected[slots[i]];
        Str line = sel >= 0 ? StoryFmt(cx, "%s: Some(\"%s\")", labels[i],
                                       lists[i][sel])
                            : StoryFmt(cx, "%s: None", labels[i]);
        valueCol->Child(StoryTxt(cx, line, 16, th.foreground));
    }
    valueCol
        ->Child(StoryTxt(cx, StrL("This is other text."), 16, th.foreground));
    StorySectionAdd(values, valueCol);
    page->Child(values);
    return page;
}

// Esc closes what this page has open, like an overlay dismiss.
void SelectStory::OnKey(SelectStory* self, Ctx* cx, const KeyEvent* ev) {
    if (ev->vk != VK_ESCAPE) {
        return;
    }
    self->open = -1;
    Notify(cx);
}

STORY_PAGE_KEYS(StorySelect, SelectStory);
