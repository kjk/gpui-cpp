#include "Story.h"

// The pages the Rust story builds, and the groups of the active one.
enum {
    SetFieldSwitch = 0,
    SetFieldCheckbox,
    SetFieldSelect,
    SetFieldNumber
};

struct SettingRow {
    const char* group;
    const char* label;
    const char* description;
    int field;
    const char* value; // for a select or number field
};

static const SettingRow kRows[] = {
    {"Appearance", "Dark Mode", "Switch between light and dark themes.",
     SetFieldSwitch, nullptr},
    {"Appearance", "Auto Switch Theme",
     "Automatically switch theme based on system settings.", SetFieldCheckbox,
     nullptr},
    {"Appearance", "resettable", "Enable/Disable reset button for settings.",
     SetFieldSwitch, nullptr},
    {"Appearance", "Group Variant", "Select the variant for setting groups.",
     SetFieldSelect, "Outline"},
    {"Appearance", "Group Size", "Select the size for the setting group.",
     SetFieldSelect, "Medium"},
    {"Font", "Font Family", "Select the font family for the story.",
     SetFieldSelect, "Arial"},
    {"Font", "Font Size",
     "Adjust the font size for better readability between 8 and 72.",
     SetFieldNumber, "14"},
    {"Font", "Line Height",
     "Adjust the line height for better readability between 0 and 100.",
     SetFieldNumber, "12"},
};

static const char* kPages[] = {"General", "Software Update", "About"};
static const char* kGeneralSubs[] = {"Appearance", "Font", "Other"};

struct SettingsStory {
    int page = 0;
    bool darkMode = false;
    bool autoSwitch = false;
    bool resettable = true;
    LineInput search = {};
    LineInput fontSize = {};
    LineInput lineHeight = {};
    StoryToolbarState toolbar;
    bool seeded = false;

    static El* Render(SettingsStory* self, Ctx* cx);
};

static void PickPage(SettingsStory* self, Ctx* cx, const ClickEvent*,
                     intptr_t ix) {
    self->page = (int)ix;
    Notify(cx);
}
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
static void FocusSettingsSearch(SettingsStory* self, Ctx* cx,
                                const ClickEvent*) {
    self->search.focused = true;
    Notify(cx);
}

El* SettingsStory::Render(SettingsStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    if (!self->seeded) {
        self->seeded = true;
        StrCopyZ(self->search.placeholder,
                 (int)sizeof(self->search.placeholder), "Search...");
        StrCopyZ(self->fontSize.buf, (int)sizeof(self->fontSize.buf), "14");
        self->fontSize.len = 2;
        StrCopyZ(self->lineHeight.buf, (int)sizeof(self->lineHeight.buf), "12");
        self->lineHeight.len = 2;
    }
    if (self->search.focused) {
        cx->win->input = &self->search;
    }
    Listener pickPage = Listen(cx, &PickPage);

    El* row = Div(a)
                  ->FlexRow()
                  ->W(kFill)
                  ->H(WindowSize(cx->win).dipH - 160)
                  ->ItemsStart();

    // The page list, with the search field above it.
    El* side = Div(a)->FlexCol()->W(160)->H(kFill)->Pad(8)->Gap(4)->BorderR(
        1, th.border);
    side->Child(
        component::Input::New(cx, StrL("settings-search"), &self->search)
            ->Prefix(Div(a)->PadL(10)->Child(IconEl(a, IconName::Search, 16)
                                                 ->Fg(th.mutedFg)))
            ->WithSize(UiSize::Small)
            ->OnFocus(Listen(cx, &FocusSettingsSearch))
            ->IntoEl());
    static const IconName kPageIcons[] = {IconName::Settings2,
                                          IconName::Building2, IconName::Info};
    for (int i = 0; i < 3; i++) {
        bool active = self->page == i;
        El* item = Div(a)
                       ->FlexRow()
                       ->W(kFill)
                       ->H(32)
                       ->PadX(8)
                       ->Gap(8)
                       ->ItemsCenter()
                       ->Radius(th.radius)
                       ->HoverBg(th.muted);
        if (active) {
            item->Bg(th.accent);
        }
        item->Child(IconEl(a, kPageIcons[i], 16)->Fg(th.foreground));
        item->Child(Div(a)->Grow()->ClipY()->Child(
            StoryTxt(cx, Str(kPages[i]), 16, th.foreground)
                ->MaxW(96)
                ->Truncate()));
        item->Child(
            IconEl(a, active ? IconName::ChevronDown : IconName::ChevronRight,
                   16)
                ->Fg(th.mutedFg));
        item->Click(HashClickId(StoryFmt(cx, "settings-page-%d", i)))
            ->OnClick(ListenerArg(pickPage, i));
        side->Child(item);
        if (active && i == 0) {
            for (int j = 0; j < 3; j++) {
                side->Child(Div(a)
                                ->FlexRow()
                                ->W(kFill)
                                ->H(32)
                                ->PadL(28)
                                ->ItemsCenter()
                                ->Radius(th.radius)
                                ->HoverBg(th.muted)
                                ->Child(StoryTxt(cx, Str(kGeneralSubs[j]), 16,
                                                 th.foreground)));
            }
        }
    }
    row->Child(side);

    // The active page: its title, then a card per group.
    El* pane = Div(a)->FlexCol()->Grow()->H(kFill)->ClipY();
    El* head = Div(a)
                   ->FlexRow()
                   ->W(kFill)
                   ->PadX(16)
                   ->PadY(12)
                   ->Gap(8)
                   ->ItemsCenter()
                   ->BorderB(1, th.border);
    head->Child(StoryTxt(cx, Str(kPages[self->page]), 20, th.foreground)
                    ->Semibold());
    head->Child(component::Button::New(cx, StrL("help"))
                    ->Ghost()
                    ->WithSize(UiSize::XSmall)
                    ->Icon(IconName::Info)
                    ->IntoEl());
    pane->Child(head);

    El* body = Div(a)->FlexCol()->W(kFill)->Pad(16)->Gap(8);
    const int nRows = (int)(sizeof(kRows) / sizeof(kRows[0]));
    for (int i = 0; i < nRows;) {
        const char* group = kRows[i].group;
        int end = i;
        while (end < nRows && strcmp(kRows[end].group, group) == 0) {
            end++;
        }
        body->Child(StoryTxt(cx, Str(group), 14, th.mutedFg)->PadY(4));
        El* card = Div(a)
                       ->FlexCol()
                       ->W(kFill)
                       ->Radius(th.radiusLg)
                       ->Border(1, th.border);
        for (int r = i; r < end; r++) {
            const SettingRow& item = kRows[r];
            El* line = Div(a)
                           ->FlexRow()
                           ->W(kFill)
                           ->PadX(16)
                           ->PadY(12)
                           ->Gap(16)
                           ->ItemsCenter()
                           ->JustifyBetween();
            if (r > i) {
                line->BorderT(1, th.border);
            }
            El* text = Div(a)->FlexCol()->Grow()->Gap(4);
            text->Child(StoryTxt(cx, Str(item.label), 16, th.foreground));
            text->Child(StoryTxt(cx, Str(item.description), 14, th.mutedFg)
                            ->Wrap());
            line->Child(text);
            switch (item.field) {
                case SetFieldSwitch: {
                    bool on = r == 2 ? self->resettable : self->darkMode;
                    Listener fn = r == 2 ? Listen(cx, &ToggleResettable)
                                         : Listen(cx, &ToggleDarkMode);
                    line->Child(
                        component::Switch::New(cx, StoryFmt(cx, "set-%d", r))
                            ->Checked(on)
                            ->OnClick(fn)
                            ->IntoEl());
                    break;
                }
                case SetFieldCheckbox:
                    line->Child(
                        component::Checkbox::New(cx, StoryFmt(cx, "set-%d", r))
                            ->Checked(self->autoSwitch)
                            ->OnClick(Listen(cx, &ToggleAutoSwitch))
                            ->IntoEl());
                    break;
                case SetFieldNumber: {
                    LineInput* state =
                        r == nRows - 1 ? &self->lineHeight : &self->fontSize;
                    line->Child(component::NumberInput::New(
                                    cx, StoryFmt(cx, "set-%d", r), state)
                                    ->W(140)
                                    ->IntoEl());
                    break;
                }
                default:
                    line->Child(
                        component::Select::New(cx, StoryFmt(cx, "set-%d", r))
                            ->Option(Str(item.value))
                            ->Selected(0)
                            ->W(140)
                            ->IntoEl());
                    break;
            }
            card->Child(line);
        }
        body->Child(card);
        i = end;
    }
    pane->Child(body);
    row->Child(pane);

    El* page = Div(a)->FlexCol()->W(kFill);
    page->Child(row);
    return page;
}

STORY_PAGE(StorySettings, SettingsStory);
