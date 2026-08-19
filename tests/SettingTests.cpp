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
    SettingItem it = Item("Dark Mode", "Switch between light and dark themes.");
    it.keywords[0] = StrL("appearance");
    it.nKeywords = 1;

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
}

static void AGroupIsShownWhenAnythingInItIs() {
    SettingGroup g;
    g.title = StrL("Appearance");
    g.items[0] = Item("Dark Mode", "Switch between themes.");
    g.items[1] = Item("Auto Switch", "Follow the system.");
    g.n = 2;

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
}

static void APageIsShownWhenAnyGroupIs() {
    SettingPage p;
    p.title = StrL("General");
    p.groups[0].title = StrL("Appearance");
    p.groups[0].items[0] = Item("Dark Mode", "Switch between themes.");
    p.groups[0].n = 1;
    p.groups[1].title = StrL("Font");
    p.groups[1].items[0] = Item("Font Size", "How big the text is.");
    p.groups[1].n = 1;
    p.n = 2;

    utassert(SettingPageMatches(&p, StrL("dark")));
    utassert(SettingPageMatches(&p, StrL("font")));
    // The page's own title is not what a search matches on; the items are.
    utassert(!SettingPageMatches(&p, StrL("general")));
    utassert(!SettingPageMatches(&p, StrL("network")));
}

void TestSetting() {
    TestSuite("setting");
    TheQueryMatchesTitleDescriptionAndKeywords();
    AGroupIsShownWhenAnythingInItIs();
    APageIsShownWhenAnyGroupIs();
}
