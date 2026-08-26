/* Ported from crates/ui/src/setting/item.rs and settings.rs.
 *
 * `SettingItem::is_match` is what the search box filters on: the title, the
 * description and the keywords, all lowercased. `filtered_pages` then drops a
 * group whose items all fell out and a page whose groups all did. */

#include "Test.h"

using namespace gpui::component;

static SettingItem Item(const char* title, const char* desc) {
    SettingItem it;
    it.title = Str(title);
    it.description = Str(desc);
    return it;
}

static void TheQueryMatchesTitleDescriptionAndKeywords() {
    Arena* a = ArenaNew();
    SettingItem it = Item("Dark Mode", "Switch between light and dark themes.");
    it.keywords.Append(a, StrL("appearance"));

    // An empty query matches everything, which is what an unfiltered list is.
    utassert(SettingItemMatches(&it, StrL("")));
    // The title, in any case.
    utassert(SettingItemMatches(&it, StrL("dark")));
    utassert(SettingItemMatches(&it, StrL("DARK")));
    utassert(SettingItemMatches(&it, StrL("Mode")));
    // The description.
    utassert(SettingItemMatches(&it, StrL("themes")));
    // And the keywords, which is the whole point of having them: nothing the
    // item shows says "appearance".
    utassert(SettingItemMatches(&it, StrL("APPEAR")));
    // Anything else does not.
    utassert(!SettingItemMatches(&it, StrL("font")));
    // A query longer than what it is matched against cannot be in it.
    utassert(!SettingItemMatches(&it, StrL("Dark Mode and then some")));
    ArenaDelete(a);
}

static void AGroupIsShownWhenAnythingInItIs() {
    Arena* a = ArenaNew();
    SettingGroup g;
    g.title = StrL("Appearance");
    g.items.Append(a, Item("Dark Mode", "Switch between themes."));
    g.items.Append(a, Item("Auto Switch", "Follow the system."));

    utassert(SettingGroupMatches(&g, StrL("")));
    utassert(SettingGroupMatches(&g, StrL("auto")));
    utassert(SettingGroupMatches(&g, StrL("system")));
    // Nothing in it matches, so the group goes — and Rust drops its header
    // and footer with it.
    utassert(!SettingGroupMatches(&g, StrL("font")));
    // An empty group has nothing to match.
    SettingGroup empty;
    utassert(!SettingGroupMatches(&empty, StrL("dark")));
    utassert(SettingGroupMatches(&empty, StrL("")));
    ArenaDelete(a);
}

static void APageIsShownWhenAnyGroupIs() {
    Arena* a = ArenaNew();
    SettingPage p;
    p.title = StrL("General");
    SettingGroup appearance;
    appearance.title = StrL("Appearance");
    appearance.items.Append(a, Item("Dark Mode", "Switch between themes."));
    SettingGroup font;
    font.title = StrL("Font");
    font.items.Append(a, Item("Font Size", "How big the text is."));
    p.groups.Append(a, appearance);
    p.groups.Append(a, font);

    utassert(SettingPageMatches(&p, StrL("dark")));
    utassert(SettingPageMatches(&p, StrL("font")));
    // The page's own title is not what a search matches on; the items are.
    utassert(!SettingPageMatches(&p, StrL("general")));
    utassert(!SettingPageMatches(&p, StrL("network")));
    ArenaDelete(a);
}

void TestSetting() {
    TestSuite("setting");
    TheQueryMatchesTitleDescriptionAndKeywords();
    AGroupIsShownWhenAnythingInItIs();
    APageIsShownWhenAnyGroupIs();
}
