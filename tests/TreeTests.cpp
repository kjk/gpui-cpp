/* Ported from crates/base/src/tree.rs.
 *
 * The four action handlers there are a few lines each over the selection and
 * the selected entry's folder state. This is that table. */

#include "Test.h"

static void TheKeysMapToTheActions() {
    utassert(TreeActionForKey(KeyUp) == TreeAction::SelectPrev);
    utassert(TreeActionForKey(KeyDown) == TreeAction::SelectNext);
    utassert(TreeActionForKey(KeyLeft) == TreeAction::Collapse);
    utassert(TreeActionForKey(KeyRight) == TreeAction::Expand);
    // Confirm has a handler in the tree (on_action_confirm, which toggles the
    // selected folder); enter is what carries it, as it does for the list.
    utassert(TreeActionForKey(KeyReturn) == TreeAction::Confirm);
    utassert(TreeActionForKey(KeySpace) == TreeAction::None);
}

static void TheSelectionWraps() {
    utassert(TreeSelectNext(0, 4) == 1);
    utassert(TreeSelectNext(3, 4) == 0);
    utassert(TreeSelectPrev(1, 4) == 0);
    utassert(TreeSelectPrev(0, 4) == 3);
}

static void NoSelectionCountsAsZeroBeforeStepping() {
    // Rust takes selected.unwrap_or(0) and *then* steps, so the two
    // directions land in different places from nothing: Up wraps off 0 to the
    // last entry, Down steps off 0 to the second.
    utassert(TreeSelectPrev(-1, 4) == 3);
    utassert(TreeSelectNext(-1, 4) == 1);
}

static void AnEmptyTreeHasNothingToSelect() {
    utassert(TreeSelectPrev(-1, 0) == -1);
    utassert(TreeSelectNext(-1, 0) == -1);
    // One entry is its own neighbour in both directions.
    utassert(TreeSelectPrev(0, 1) == 0);
    utassert(TreeSelectNext(0, 1) == 0);
}

static void LeftAndRightEachActInOneDirectionOnly() {
    // Left closes what is open; it does not open what is closed.
    utassert(TreeCollapses(true, true));
    utassert(!TreeCollapses(true, false));
    // Right opens what is closed; it does not close what is open.
    utassert(TreeExpands(true, false));
    utassert(!TreeExpands(true, true));
    // Neither touches a leaf.
    utassert(!TreeCollapses(false, true));
    utassert(!TreeExpands(false, false));
}

// A root with two children, the second of which has one of its own.
static void Seed(TreeState* s) {
    int root = TreeAddItem(s, StrL("root"), StrL("root"), -1);
    TreeAddItem(s, StrL("a"), StrL("a"), root);
    int b = TreeAddItem(s, StrL("b"), StrL("b"), root);
    TreeAddItem(s, StrL("b1"), StrL("b1"), b);
    int other = TreeAddItem(s, StrL("other"), StrL("other"), -1);
    (void)other;
    TreeRebuild(s);
}

static void OnlyOpenFoldersPutTheirChildrenOnScreen() {
    TreeState s;
    Seed(&s);
    // Everything starts closed, so the two roots are the whole list.
    utassert(s.nEntries == 2);
    utassert(TreeIndexOf(&s, StrL("root")) == 0);
    utassert(TreeIndexOf(&s, StrL("a")) == -1);
    // An item something else calls its parent is a folder; the rest are not.
    utassert(s.items[0].folder);
    utassert(!s.items[1].folder);
    utassert(s.items[3].depth == 2);

    bool expanded = false;
    utassert(TreeToggleExpandAt(&s, 0, &expanded));
    utassert(expanded);
    // The root's own children come in, but not the ones under the folder
    // that is still closed.
    utassert(s.nEntries == 4);
    utassert(TreeIndexOf(&s, StrL("b")) == 2);
    utassert(TreeIndexOf(&s, StrL("b1")) == -1);
    utassert(TreeIndexOf(&s, StrL("other")) == 3);

    utassert(TreeToggleExpandAt(&s, 2, &expanded));
    utassert(expanded);
    utassert(s.nEntries == 5);
    utassert(TreeIndexOf(&s, StrL("b1")) == 3);

    // Closing the root takes the whole subtree off screen at once.
    utassert(TreeToggleExpandAt(&s, 0, &expanded));
    utassert(!expanded);
    utassert(s.nEntries == 2);
}

static void ALeafDoesNotToggle() {
    TreeState s;
    Seed(&s);
    TreeToggleExpandAt(&s, 0, nullptr);
    // Entry 1 is the leaf `a`.
    utassert(!TreeToggleExpandAt(&s, 1, nullptr));
    // And neither does a row that is not there.
    utassert(!TreeToggleExpandAt(&s, 99, nullptr));
    utassert(s.nEntries == 4);
}

static void RevealOpensEveryFolderAboveIt() {
    TreeState s;
    Seed(&s);
    // `b1` is two folders deep and has no row at all to begin with.
    utassert(TreeIndexOf(&s, StrL("b1")) == -1);
    utassert(TreeRevealItem(&s, StrL("b1")) == 3);
    utassert(s.items[0].expanded);
    utassert(s.items[2].expanded);
    // An id the tree does not hold reveals nothing.
    utassert(TreeRevealItem(&s, StrL("nope")) == -1);
}

static void CollapsingPastTheSelectionPullsItBack() {
    TreeState s;
    Seed(&s);
    TreeRevealItem(&s, StrL("b1"));
    s.selected = 3;
    // The rows the selection pointed at are gone, so it lands on the last
    // one that is left rather than off the end.
    TreeToggleExpandAt(&s, 0, nullptr);
    utassert(s.nEntries == 2);
    utassert(s.selected == 1);
}

void TestTree() {
    TestSuite("tree");
    TheKeysMapToTheActions();
    TheSelectionWraps();
    NoSelectionCountsAsZeroBeforeStepping();
    AnEmptyTreeHasNothingToSelect();
    LeftAndRightEachActInOneDirectionOnly();
    OnlyOpenFoldersPutTheirChildrenOnScreen();
    ALeafDoesNotToggle();
    RevealOpensEveryFolderAboveIt();
    CollapsingPastTheSelectionPullsItBack();
}
