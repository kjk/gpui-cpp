/* Ported from crates/ui/src/searchable_list.
 *
 * A click on a row is worked out into atomic changes by the mode — Single
 * replaces the selection, Multi toggles the row — and the delegate's default
 * `on_will_change` then applies them: a Select whose value is already
 * selected changes nothing, and a Deselect takes out whatever carries that
 * value, falling back to the index when nothing does. */

#include "Test.h"

using namespace gpui::component;

static const SearchableItem kItems[] = {
    {StrL("Apple"), StrL("apple"), 0, false},
    {StrL("Banana"), StrL("banana"), 0, false},
    {StrL("Cherry"), StrL("cherry"), 0, true},
    // Two rows for the same value, which is what makes "by value" and "by
    // index" different answers.
    {StrL("Apple (again)"), StrL("apple"), 1, false},
};
static const int kN = 4;

static void Apply(SearchableListState* s, int index) {
    SearchableListChange changes[kMaxSearchableSelection + 1];
    int n = SearchableListChangesFor(s, kItems, kN, index, changes,
                                     (int)(sizeof(changes) / sizeof(*changes)));
    SearchableListApply(s, kItems, kN, changes, n);
}

static void TheQueryIsACaseInsensitiveSubstringOfTheTitle() {
    utassert(SearchableItemMatches(&kItems[0], StrL("app")));
    utassert(SearchableItemMatches(&kItems[0], StrL("APPLE")));
    utassert(SearchableItemMatches(&kItems[0], StrL("ple")));
    utassert(!SearchableItemMatches(&kItems[0], StrL("banana")));
    // An empty query matches everything, as `contains("")` does.
    utassert(SearchableItemMatches(&kItems[1], StrL("")));
}

static void SearchLeavesTheMatchesInOrder() {
    SearchableListState s;
    SearchableListSearch(&s, kItems, kN, StrL("an"));
    // Banana, and nothing else.
    utassert(s.nMatches == 1);
    utassert(s.matches[0] == 1);
    // The list is told how many rows it has, since that is what its keys walk.
    utassert(s.list.count == 1);

    SearchableListSearch(&s, kItems, kN, StrL(""));
    utassert(s.nMatches == kN);
    utassert(s.matches[3] == 3);
}

static void SingleReplacesTheSelection() {
    SearchableListState s;
    Apply(&s, 0);
    utassert(s.nSelected == 1 && s.selected[0] == 0);
    // Picking another one deselects what was there first.
    Apply(&s, 1);
    utassert(s.nSelected == 1 && s.selected[0] == 1);
    // Picking the same one again leaves it selected: the Deselect takes it
    // out and the Select puts it straight back.
    Apply(&s, 1);
    utassert(s.nSelected == 1 && s.selected[0] == 1);
}

static void MultiTogglesOnlyTheRowThatWasClicked() {
    SearchableListState s;
    s.mode = SearchableListMode::Multi;
    Apply(&s, 0);
    Apply(&s, 1);
    utassert(s.nSelected == 2 && s.selected[0] == 0 && s.selected[1] == 1);
    // The second click on the first row takes only that one out.
    Apply(&s, 0);
    utassert(s.nSelected == 1 && s.selected[0] == 1);
}

static void ASelectionIsComparedByValue() {
    SearchableListState s;
    s.mode = SearchableListMode::Multi;
    Apply(&s, 0);
    // Row 3 carries the same value as row 0, so it is already checked...
    utassert(SearchableListIsChecked(&s, kItems, kN, 3));
    // ...and selecting it adds nothing.
    Apply(&s, 3);
    utassert(s.nSelected == 0);
    // Which is to say the toggle deselected it: a value already in the
    // selection is what the click was toggling.
    utassert(!SearchableListIsChecked(&s, kItems, kN, 0));
}

void TestSearchableList() {
    TestSuite("searchable_list");
    TheQueryIsACaseInsensitiveSubstringOfTheTitle();
    SearchLeavesTheMatchesInOrder();
    SingleReplacesTheSelection();
    MultiTogglesOnlyTheRowThatWasClicked();
    ASelectionIsComparedByValue();
}
