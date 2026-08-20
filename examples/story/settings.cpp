#include "Story.h"

// The pages the Rust story builds: a sidebar of pages, groups inside the
// active one, and an item per setting. The component owns all of that now —
// what is left here is the values behind the fields and the fields
// themselves, which is what the Rust story's `SettingField`s stand for.
// The two dropdowns' items. A SearchableList keeps a pointer to them, so
// they outlive the frame.
static const component::SearchableItem kFontFamilies[] = {
    {StrL("Arial"), StrL("arial"), 0, false},
    {StrL("Consolas"), StrL("consolas"), 0, false},
    {StrL("Segoe UI"), StrL("segoe"), 0, false},
};
static const component::SearchableItem kGroupVariants[] = {
    {StrL("Outline"), StrL("outline"), 0, false},
    {StrL("Fill"), StrL("fill"), 0, false},
    {StrL("Plain"), StrL("plain"), 0, false},
};

struct SettingsStory {
    Entity<component::SettingsState> settings = {};
    bool darkMode = false;
    bool autoSwitch = false;
    bool resettable = true;
    Entity<component::SearchableListState> fontFamily = {};
    Entity<component::SearchableListState> groupVariant = {};
    InputState search;
    InputState fontSize;
    InputState lineHeight;
    bool seeded = false;

    static El* Render(SettingsStory* self, Ctx* cx);
};

static void ToggleDarkMode(SettingsStory* self, Ctx* cx, const ClickEvent*) {
    self->darkMode = !self->darkMode;
    Notify(cx);
}
static void ToggleAutoSwitch(SettingsStory* self, Ctx* cx, const ClickEvent*) {
    self->autoSwitch = !self->autoSwitch;
    Notify(cx);
}
static void ToggleResettable(SettingsStory* self, Ctx* cx, const ClickEvent*) {
    self->resettable = !self->resettable;
    Notify(cx);
}
// The select owns its selection now, so a toggle is all the page does.
static void ToggleFontFamily(SettingsStory* self, Ctx* cx, const ClickEvent*) {
    component::SelectToggleOpen(self->fontFamily.Get(cx), cx);
}
static void ToggleGroupVariant(SettingsStory* self, Ctx* cx,
                               const ClickEvent*) {
    component::SelectToggleOpen(self->groupVariant.Get(cx), cx);
}
// on_reset: the font size goes back to what it started at.
static void ResetFontSize(SettingsStory* self, Ctx* cx, const ClickEvent*) {
    InputSetValue(&self->fontSize, StrL("14"));
    Notify(cx);
}
static void FocusSettingsSearch(SettingsStory* self, Ctx* cx,
                                const ClickEvent*) {
    self->search.focused = true;
    Notify(cx);
}

El* SettingsStory::Render(SettingsStory* self, Ctx* cx) {
    Arena* a = cx->a;
    if (!self->seeded) {
        self->seeded = true;
        InputSetPlaceholder(&self->search, StrL("Search..."));
        InputSetValue(&self->fontSize, StrL("14"));
        InputSetValue(&self->lineHeight, StrL("12"));
        self->settings = EntityNewState<component::SettingsState>(cx->app);
        self->fontFamily =
            EntityNewState<component::SearchableListState>(cx->app);
        self->groupVariant =
            EntityNewState<component::SearchableListState>(cx->app);
        component::SearchableListState* ff = self->fontFamily.Get(cx);
        component::SearchableListState* gv = self->groupVariant.Get(cx);
        if (ff) {
            ff->nSelected = 1;
        }
        if (gv) {
            gv->nSelected = 1;
        }
    }
    if (self->search.focused) {
        cx->win->input = &self->search;
    }

    component::Settings* s =
        component::Settings::New(cx, StrL("settings"), self->settings)
            ->Searchable(&self->search, Listen(cx, &FocusSettingsSearch))
            ->SidebarWidth(200)
            ->H(WindowSize(cx->win).dipH - 160);

    s->Page(StrL("General"), IconName::Settings2);
    s->Group(StrL("Appearance"));
    s->Item(StrL("Dark Mode"), StrL("Switch between light and dark themes."),
            component::Switch::New(cx, StrL("set-dark"))
                ->Checked(self->darkMode)
                ->OnClick(Listen(cx, &ToggleDarkMode))
                ->IntoEl());
    s->Keywords(StrL("theme"), StrL("night"));
    s->Item(StrL("Auto Switch Theme"),
            StrL("Automatically switch theme based on system settings."),
            component::Checkbox::New(cx, StrL("set-auto"))
                ->Checked(self->autoSwitch)
                ->OnClick(Listen(cx, &ToggleAutoSwitch))
                ->IntoEl());
    s->Keywords(StrL("theme"), StrL("system"));
    s->Item(StrL("resettable"),
            StrL("Enable/Disable reset button for settings."),
            component::Switch::New(cx, StrL("set-resettable"))
                ->Checked(self->resettable)
                ->OnClick(Listen(cx, &ToggleResettable))
                ->IntoEl());
    s->Item(StrL("Group Variant"),
            StrL("Select the variant for setting groups."),
            component::Select::New(cx, StrL("set-variant"), self->groupVariant)
                ->Items(kGroupVariants, 3)
                ->W(140)
                ->OnToggle(Listen(cx, &ToggleGroupVariant))
                ->IntoEl());

    s->Group(StrL("Font"));
    s->Item(StrL("Font Family"), StrL("Select the font family for the story."),
            component::Select::New(cx, StrL("set-family"), self->fontFamily)
                ->Items(kFontFamilies, 3)
                ->W(140)
                ->OnToggle(Listen(cx, &ToggleFontFamily))
                ->IntoEl());
    s->Keywords(StrL("typeface"));
    s->Item(
        StrL("Font Size"),
        StrL("Adjust the font size for better readability between 8 and 72."),
        component::NumberInput::New(cx, StrL("set-font-size"), &self->fontSize)
            ->W(140)
            ->IntoEl());
    // is_dirty / on_reset: a size that is no longer the default offers to go
    // back to it, which is what the story's resettable switch turns on.
    s->Resettable(
        self->resettable && !StrSame(InputValue(&self->fontSize), StrL("14")),
        Listen(cx, &ResetFontSize));
    s->Item(StrL("Line Height"),
            StrL("Adjust the line height for better readability between 0 and "
                 "100."),
            component::NumberInput::New(cx, StrL("set-line-height"),
                                        &self->lineHeight)
                ->W(140)
                ->IntoEl());

    s->Page(StrL("Software Update"), IconName::Building2);
    s->Group(StrL("Updates"));
    s->Item(StrL("Automatic Updates"),
            StrL("Download and install updates in the background."),
            component::Switch::New(cx, StrL("set-updates"))
                ->Checked(true)
                ->IntoEl());

    s->Page(StrL("About"), IconName::Info);
    s->Group(StrL("This build"));
    s->Item(StrL("Version"), StrL("The version this story was built from."),
            nullptr);

    El* page = Div(a)->FlexCol()->W(kFill);
    page->Child(s->IntoEl());
    return page;
}

STORY_PAGE(StorySettings, SettingsStory);
