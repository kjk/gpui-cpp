/* Ported from crates/ui/src/native_menu.
 *
 * The builder puts one row per call, carrying its label, its disabled and
 * checked state, its icon and what it reports. What the OS is handed is
 * numbered over the rows that can be chosen — preorder, skipping separators,
 * submenu rows and greyed rows — which is Rust's `actions` vector and what
 * makes the id the OS answers with map back to the row that was built. */

#include "Test.h"

using namespace gpui::component;

// test_native_menu_builder_accepts_icon: the row carries what it was built
// with, icon included.
static void ARowCarriesWhatItWasBuiltWith() {
    NativeMenu m;
    utassert(m.IsEmpty());
    m.MenuWithIcon(StrL("Github"), IconName::Github, 7);
    utassert(!m.IsEmpty());
    utassert(m.n == 1);
    utassert(m.items[0].kind == NativeMenuItemKind::Item);
    utassert(StrEqI(m.items[0].label, StrL("Github")));
    utassert(!m.items[0].disabled);
    utassert(!m.items[0].checked);
    utassert(m.items[0].icon == IconName::Github);
    utassert(m.items[0].id == 7);

    // menu_with_disabled and menu_with_check each set one of the two.
    m.MenuWithDisabled(StrL("Inbox"), true, 8);
    m.MenuWithCheck(StrL("Wrap"), true, 9);
    utassert(m.items[1].disabled && !m.items[1].checked);
    utassert(m.items[2].checked && !m.items[2].disabled);

    m.Separator();
    utassert(m.items[3].kind == NativeMenuItemKind::Separator);

    NativeMenu sub;
    sub.Menu(StrL("Copy"), 10);
    m.Submenu(StrL("Edit"), &sub);
    utassert(m.items[4].kind == NativeMenuItemKind::Submenu);
    utassert(m.items[4].submenu == &sub);
    utassert(m.n == 5);
}

// A menu only holds so many rows; the ones past that are dropped rather than
// written past the end of it.
static void TheRowsPastTheEndAreDropped() {
    NativeMenu m;
    for (int i = 0; i < kNativeMenuMaxItems + 4; i++) {
        m.Menu(StrL("Item"), i);
    }
    utassert(m.n == kNativeMenuMaxItems);
    utassert(m.items[kNativeMenuMaxItems - 1].id == kNativeMenuMaxItems - 1);
}

// The table the id maps back through: 1-based over what can be chosen, in the
// order the rows are built, with a submenu's rows taken where it sits.
static void OnlyTheRowsThatCanBeChosenAreNumbered() {
    NativeMenu sub;
    sub.Menu(StrL("Copy"), 20);
    sub.Menu(StrL("Cut"), 21);
    NativeMenu m;
    m.Menu(StrL("New"), 1);
    m.Separator();
    m.MenuWithDisabled(StrL("Save"), true, 2);
    m.Submenu(StrL("Edit"), &sub);
    m.Menu(StrL("Quit"), 3);

    const NativeMenuItem* table[8] = {};
    int n = NativeMenuSelectable(&m, table, 8);
    utassert(n == 4);
    utassert(table[0]->id == 1);
    utassert(table[1]->id == 20);
    utassert(table[2]->id == 21);
    utassert(table[3]->id == 3);

    // Counting works without a table to write into, and a menu that is not
    // there has nothing to count.
    utassert(NativeMenuSelectable(&m, nullptr, 0) == 4);
    utassert(NativeMenuSelectable(nullptr, table, 8) == 0);
}

// A greyed submenu row still has its rows numbered: Win32 greys the row that
// opens the submenu, not what is inside it.
static void AGreyedSubmenuStillNumbersItsRows() {
    NativeMenu sub;
    sub.Menu(StrL("Copy"), 30);
    NativeMenu m;
    m.Submenu(StrL("Edit"), &sub);
    m.items[0].disabled = true;
    const NativeMenuItem* table[4] = {};
    utassert(NativeMenuSelectable(&m, table, 4) == 1);
    utassert(table[0]->id == 30);
}

// An empty menu has nothing to show, which is what keeps `show` from opening
// a popup with no rows in it.
static void AnEmptyMenuShowsNothing() {
    NativeMenu m;
    utassert(m.IsEmpty());
    utassert(!m.Show(0, 0));
    const NativeMenuItem* table[4] = {};
    utassert(NativeMenuSelectable(&m, table, 4) == 0);
}

void TestNativeMenu() {
    TestSuite("native_menu");
    ARowCarriesWhatItWasBuiltWith();
    TheRowsPastTheEndAreDropped();
    OnlyTheRowsThatCanBeChosenAreNumbered();
    AGreyedSubmenuStillNumbersItsRows();
    AnEmptyMenuShowsNothing();
}
