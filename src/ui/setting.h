/* Themed settings — crates/ui/src/setting

   Rust's Settings is a sidebar of pages, each page a list of groups, each
   group a list of items; an item is a title, a description and a field, and
   the search box at the top of the sidebar filters the whole tree by what an
   item says. The fields themselves are the caller's here — Rust builds them
   from the type of the value behind the setting, which needs a reflection
   table this tree has no use for. */

#include "ui/sizing.h"

namespace gpui {

namespace component {

const int kMaxSettingItems = 16;
const int kMaxSettingGroups = 8;
const int kMaxSettingPages = 8;
const int kMaxSettingKeywords = 4;

// SettingItem::Item: the title, what it is for, and the control that changes
// it. `keywords` is what the search box matches on beyond the two strings.
struct SettingItem {
    Str title = {};
    Str description = {};
    El* control = nullptr;
    Str keywords[kMaxSettingKeywords] = {};
    int nKeywords = 0;
    bool disabled = false;
    // is_resettable / on_reset: an item that has been changed shows a reset
    // button beside it.
    bool dirty = false;
    Listener onReset = {};
    // layout(Axis): Horizontal puts the control beside the text, Vertical
    // under it.
    Axis layout = Axis::Horizontal;
};

struct SettingGroup {
    Str title = {};
    Str description = {};
    SettingItem items[kMaxSettingItems] = {};
    int n = 0;
};

struct SettingPage {
    Str title = {};
    Str description = {};
    IconName icon = IconName::None;
    SettingGroup groups[kMaxSettingGroups] = {};
    int n = 0;
};

// is_match: the query against the title, the description and the keywords,
// case-insensitively. An empty query matches everything.
bool SettingItemMatches(const SettingItem* it, Str query);
bool SettingGroupMatches(const SettingGroup* g, Str query);
bool SettingPageMatches(const SettingPage* p, Str query);

// SettingsState: which page is showing, which group the sidebar last jumped
// to, and nothing else — the values behind the fields are the caller's.
struct SettingsState {
    int page = 0;
    int group = -1;

    static void OnPageClick(SettingsState* self, Ctx* cx, const ClickEvent* ev,
                            intptr_t page);
    static void OnGroupClick(SettingsState* self, Ctx* cx, const ClickEvent* ev,
                             intptr_t packed);
};

struct Settings {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<SettingsState> state = {};
    SettingPage pages[kMaxSettingPages] = {};
    int n = 0;
    // The search field, which filters the pages, the groups and the items.
    InputState* search = nullptr;
    Listener onSearchFocus = {};
    float sidebarWidth = 220;
    float h = 480;
    // GroupBoxVariant: whether a group is a card with a border or a plain
    // run of rows under a heading.
    bool bordered = true;

    static Settings* New(Ctx* cx, Str id, Entity<SettingsState> state);
    Settings* Page(Str title, IconName icon = IconName::None,
                   Str description = {});
    Settings* Group(Str title, Str description = {});
    Settings* Item(Str title, Str description, El* control);
    // The item last added: its keywords, whether it is disabled, and what a
    // reset does.
    Settings* Keywords(Str a1, Str a2 = {}, Str a3 = {});
    Settings* Disabled(bool v = true);
    Settings* Resettable(bool dirty, Listener onReset);
    Settings* Layout(Axis axis);
    Settings* Searchable(InputState* search, Listener onFocus);
    Settings* SidebarWidth(float v);
    Settings* H(float v);
    Settings* Bordered(bool v);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
