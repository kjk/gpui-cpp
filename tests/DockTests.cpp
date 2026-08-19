/* Ported from crates/ui/src/dock/tab_panel.rs.
 *
 * `split_placement_at` picks one of five zones from a position inside a tab
 * panel, and `DropPlaceholderBounds::for_placement` turns that answer into the
 * half of the panel a drop would take —
 * drop_placeholder_bounds_cover_each_target_placement is the Rust test for it.
 * The rest is what the tree does when a drop lands: a merge, a split, and the
 * pruning a group left empty triggers (remove_self_if_empty). */

#include "Test.h"

static void TheFiveDropZones() {
    Bounds b = {0, 0, 200, 100};
    // Left of 35%, right of 65%, then the same two thresholds vertically —
    // the horizontal ones are asked first, as in Rust.
    utassert(DockDropAt(b, 10, 50) == DockDrop::Left);
    utassert(DockDropAt(b, 190, 50) == DockDrop::Right);
    utassert(DockDropAt(b, 100, 10) == DockDrop::Top);
    utassert(DockDropAt(b, 100, 90) == DockDrop::Bottom);
    utassert(DockDropAt(b, 100, 50) == DockDrop::Center);
    // A corner is an edge, and the horizontal answer wins it.
    utassert(DockDropAt(b, 10, 10) == DockDrop::Left);
    // The bounds are not assumed to start at the origin.
    Bounds off = {100, 200, 200, 100};
    utassert(DockDropAt(off, 110, 250) == DockDrop::Left);
    utassert(DockDropAt(off, 200, 250) == DockDrop::Center);
}

static void ThePlaceholderCoversEachZone() {
    Bounds b = {0, 0, 200, 100};
    Bounds left = DockDropPlaceholder(b, DockDrop::Left);
    utassert(left.x == 0 && left.y == 0 && left.w == 100 && left.h == 100);
    Bounds right = DockDropPlaceholder(b, DockDrop::Right);
    utassert(right.x == 100 && right.y == 0 && right.w == 100 &&
             right.h == 100);
    Bounds top = DockDropPlaceholder(b, DockDrop::Top);
    utassert(top.x == 0 && top.y == 0 && top.w == 200 && top.h == 50);
    Bounds bottom = DockDropPlaceholder(b, DockDrop::Bottom);
    utassert(bottom.x == 0 && bottom.y == 50 && bottom.w == 200 &&
             bottom.h == 50);
    // A merge covers the whole panel, which is what says "no split".
    Bounds centre = DockDropPlaceholder(b, DockDrop::Center);
    utassert(centre.x == 0 && centre.y == 0 && centre.w == 200 &&
             centre.h == 100);
}

// Two tab groups side by side in one split, with two panels in the first.
static void Seed(DockState* s, int* a, int* b) {
    for (int i = 0; i < 3; i++) {
        DockPanelDef def;
        def.title = StrL("panel");
        DockAddPanelDef(s, def);
    }
    *a = DockNewTabs(s);
    DockTabsAdd(s, *a, 0);
    DockTabsAdd(s, *a, 1);
    *b = DockNewTabs(s);
    DockTabsAdd(s, *b, 2);
    int split = DockNewSplit(s, Axis::Horizontal);
    DockSplitAdd(s, split, *a, 300);
    DockSplitAdd(s, split, *b, 300);
    s->center = split;
    s->nodes[*a].bounds = {0, 0, 300, 400};
    s->nodes[*b].bounds = {300, 0, 300, 400};
}

static void ADropInTheMiddleMerges() {
    DockState s;
    int a = 0, b = 0;
    Seed(&s, &a, &b);
    utassert(DockMovePanelTo(&s, 1, b, DockDrop::Center));
    utassert(s.nodes[a].nPanel == 1);
    utassert(s.nodes[b].nPanel == 2);
    // The panel that arrived is the one showing.
    utassert(s.nodes[b].activeIx == 1);
    utassert(DockNodeOfPanel(&s, 1) == b);
    // Moving a panel onto the group it is already in changes nothing.
    utassert(!DockMovePanelTo(&s, 1, b, DockDrop::Center));
}

static void ADropOnAnEdgeSplits() {
    DockState s;
    int a = 0, b = 0;
    Seed(&s, &a, &b);
    int split = s.center;
    utassert(DockMovePanelTo(&s, 0, b, DockDrop::Right));
    // The split already ran this way, so the panel joined it rather than
    // nesting a second split inside it.
    utassert(s.center == split);
    utassert(s.nodes[split].nChild == 3);
    int fresh = s.nodes[split].child[2];
    utassert(s.nodes[fresh].nPanel == 1 && s.nodes[fresh].panel[0] == 0);
    // The two share what the target had.
    utassert(s.nodes[split].size[1] == 150);
    utassert(s.nodes[split].size[2] == 150);

    // Across the axis it does nest: a vertical drop inside a horizontal split
    // puts a new split where the target was.
    DockState s2;
    Seed(&s2, &a, &b);
    utassert(DockMovePanelTo(&s2, 0, b, DockDrop::Bottom));
    int outer = s2.center;
    utassert(s2.nodes[outer].nChild == 2);
    int nested = s2.nodes[outer].child[1];
    utassert(s2.nodes[nested].split);
    utassert(!AxisIsHorizontal(s2.nodes[nested].axis));
    // Bottom means after the target.
    utassert(s2.nodes[nested].child[0] == b);
    utassert(s2.nodes[s2.nodes[nested].child[1]].panel[0] == 0);
}

static void AnEmptyGroupLeavesTheSplit() {
    DockState s;
    int a = 0, b = 0;
    Seed(&s, &a, &b);
    int split = s.center;
    // Take both of the first group's panels away and the group goes with the
    // second one, then the split with one child left becomes that child.
    utassert(DockClosePanelAt(&s, a, 0));
    utassert(s.nodes[a].used);
    utassert(s.nodes[a].nPanel == 1);
    utassert(DockClosePanelAt(&s, a, 0));
    utassert(!s.nodes[a].used);
    utassert(!s.nodes[split].used);
    utassert(s.center == b);
    utassert(s.nodes[b].parent == -1);
}

static void ARootGroupStays() {
    DockState s;
    DockPanelDef def;
    def.title = StrL("panel");
    DockAddPanelDef(&s, def);
    int node = DockNewTabs(&s);
    DockTabsAdd(&s, node, 0);
    s.left.node = node;
    // The Dock on a side keeps its empty tab group — Rust says so by making
    // that TabPanel `closable = false`.
    utassert(DockClosePanelAt(&s, node, 0));
    utassert(s.nodes[node].used);
    utassert(s.nodes[node].nPanel == 0);
    utassert(s.left.node == node);
}

static void ALockedDockMovesNothing() {
    DockState s;
    int a = 0, b = 0;
    Seed(&s, &a, &b);
    s.locked = true;
    utassert(!DockMovePanelTo(&s, 1, b, DockDrop::Center));
    utassert(s.nodes[a].nPanel == 2);
}

void TestDock() {
    TheFiveDropZones();
    ThePlaceholderCoversEachZone();
    ADropInTheMiddleMerges();
    ADropOnAnEdgeSplits();
    AnEmptyGroupLeavesTheSplit();
    ARootGroupStays();
    ALockedDockMovesNothing();
}
