/* Ported from crates/base/src/tree.rs.
 *
 * The four action handlers there are a few lines each over the selection and
 * the selected entry's folder state. This is that table. */

#include "Test.h"

static void TheFourKeysMapToTheFourActions() {
    utassert(TreeActionForKey(KeyUp) == TreeAction::SelectPrev);
    utassert(TreeActionForKey(KeyDown) == TreeAction::SelectNext);
    utassert(TreeActionForKey(KeyLeft) == TreeAction::Collapse);
    utassert(TreeActionForKey(KeyRight) == TreeAction::Expand);
    utassert(TreeActionForKey(KeyReturn) == TreeAction::None);
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

void TestTree() {
    TestSuite("tree");
    TheFourKeysMapToTheFourActions();
    TheSelectionWraps();
    NoSelectionCountsAsZeroBeforeStepping();
    AnEmptyTreeHasNothingToSelect();
    LeftAndRightEachActInOneDirectionOnly();
}
