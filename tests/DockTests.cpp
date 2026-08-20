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
    utassert(s.nodes[a].panel.len == 1);
    utassert(s.nodes[b].panel.len == 2);
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
    utassert(s.nodes[split].child.len == 3);
    int fresh = s.nodes[split].child[2];
    utassert(s.nodes[fresh].panel.len == 1 && s.nodes[fresh].panel[0] == 0);
    // The two share what the target had.
    utassert(s.nodes[split].size[1] == 150);
    utassert(s.nodes[split].size[2] == 150);

    // Across the axis it does nest: a vertical drop inside a horizontal split
    // puts a new split where the target was.
    DockState s2;
    Seed(&s2, &a, &b);
    utassert(DockMovePanelTo(&s2, 0, b, DockDrop::Bottom));
    int outer = s2.center;
    utassert(s2.nodes[outer].child.len == 2);
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
    utassert(s.nodes[a].panel.len == 1);
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
    utassert(s.nodes[node].panel.len == 0);
    utassert(s.left.node == node);
}

// insert_panel_at: a drop that landed on a tab takes that tab's place in the
// row, which is what reorders a group's own tabs.
static void ADropOnATabTakesItsPlace() {
    DockState s;
    int a = 0, b = 0;
    Seed(&s, &a, &b);
    // The second tab of A dropped on the first: it goes in front of it.
    utassert(DockMovePanelTo(&s, 1, a, DockDrop::Center, 0));
    utassert(s.nodes[a].panel.len == 2);
    utassert(s.nodes[a].panel[0] == 1);
    utassert(s.nodes[a].panel[1] == 0);
    // insert_panel_at ends with set_active_ix: what was dropped is showing.
    utassert(s.nodes[a].activeIx == 0);

    // A panel from another group inserted at a place in this one.
    utassert(DockMovePanelTo(&s, 2, a, DockDrop::Center, 1));
    utassert(s.nodes[a].panel.len == 3);
    utassert(s.nodes[a].panel[1] == 2);
    utassert(DockNodeOfPanel(&s, 2) == a);
    // Its old group emptied and left the split, so the split is gone with it.
    utassert(!s.nodes[b].used);
}

// The index is worked out before the panel is detached, which is Rust's
// order: a tab dragged rightwards inside its own row lands one place short of
// where it was let go.
static void AReorderCountsFromBeforeTheDetach() {
    DockState s;
    for (int i = 0; i < 4; i++) {
        DockPanelDef def;
        def.title = StrL("panel");
        DockAddPanelDef(&s, def);
    }
    int node = DockNewTabs(&s);
    for (int i = 0; i < 4; i++) {
        DockTabsAdd(&s, node, i);
    }
    s.center = node;
    // The first tab dropped on the third: 0 comes out, and 2 is where 3 was.
    utassert(DockMovePanelTo(&s, 0, node, DockDrop::Center, 2));
    utassert(s.nodes[node].panel[0] == 1);
    utassert(s.nodes[node].panel[1] == 2);
    utassert(s.nodes[node].panel[2] == 0);
    utassert(s.nodes[node].panel[3] == 3);
}

// The two same-group rules: a drop with no tab named is nothing, and a group
// of one dropped on its own edge is the layout it already has.
static void ADropOnItsOwnGroupNeedsATabOrAnEdge() {
    DockState s;
    int a = 0, b = 0;
    Seed(&s, &a, &b);
    utassert(!DockMovePanelTo(&s, 0, a, DockDrop::Center));
    // B holds one panel, so splitting it with itself is refused.
    utassert(!DockMovePanelTo(&s, 2, b, DockDrop::Right));
    // A holds two, so one of them can split it.
    utassert(DockMovePanelTo(&s, 1, a, DockDrop::Right));
}

// Which Dock a node belongs to, which is what a click on a collapsed one has
// to know before it can open it again.
static void ANodeKnowsWhichDockItIsIn() {
    DockState s;
    int a = 0, b = 0;
    Seed(&s, &a, &b);
    int side = DockNewTabs(&s);
    s.bottom.node = side;
    utassert(DockPlacementOfNode(&s, a) == DockPlacement::Center);
    utassert(DockPlacementOfNode(&s, side) == DockPlacement::Bottom);
    // A group inside a split inside a Dock is still in that Dock.
    int inner = DockNewTabs(&s);
    int split = DockNewSplit(&s, Axis::Vertical);
    DockSplitAdd(&s, split, inner, 100);
    s.left.node = split;
    utassert(DockPlacementOfNode(&s, inner) == DockPlacement::Left);
}

// ScrollHandle::scroll_to_item: a tab off either end of the bar is brought
// just inside it, and one already inside asks for nothing.
static void ATabOffTheEndIsBroughtIntoView() {
    Bounds strip = {100, 0, 200, 30};
    // Already inside.
    utassertnear(DockTabScrollTo(40, strip, Bounds{120, 0, 60, 30}), 40.f);
    // Off the right edge by 50: the offset grows by exactly that.
    utassertnear(DockTabScrollTo(40, strip, Bounds{260, 0, 90, 30}), 90.f);
    // Off the left edge by 30: the offset shrinks by exactly that.
    utassertnear(DockTabScrollTo(40, strip, Bounds{70, 0, 60, 30}), 10.f);
    // Nothing measured yet is nothing to scroll to.
    utassertnear(DockTabScrollTo(40, strip, Bounds{}), 40.f);
}

// DockArea::dump and load: the tree written out and built back, with the
// panels matched by the name they were registered under.
static void ALayoutSurvivesDumpAndLoad() {
    Arena* arena = ArenaNew();
    DockState s;
    static const char* kNames[] = {"AlphaPanel", "BetaPanel", "GammaPanel"};
    for (int i = 0; i < 3; i++) {
        DockPanelDef def;
        def.name = Str(kNames[i]);
        def.title = Str(kNames[i]);
        DockAddPanelDef(&s, def);
    }
    int tabs = DockNewTabs(&s);
    DockTabsAdd(&s, tabs, 0);
    DockTabsAdd(&s, tabs, 1);
    s.nodes[tabs].activeIx = 1;
    int other = DockNewTabs(&s);
    DockTabsAdd(&s, other, 2);
    int split = DockNewSplit(&s, Axis::Vertical);
    DockSplitAdd(&s, split, tabs, 240);
    DockSplitAdd(&s, split, other, 160);
    s.center = split;
    int sideNode = DockNewTabs(&s);
    DockTabsAdd(&s, sideNode, 2);
    s.left.node = sideNode;
    s.left.size = 210;
    s.left.open = false;

    DockAreaState state;
    DockDump(&s, &state);
    // The tree is PanelStates: a split is a StackPanel, a group a TabPanel,
    // and a panel a leaf under its registered name.
    utassert(StrEqI(state.nodes[state.center].panelName, StrL("StackPanel")));
    utassert(state.nodes[state.center].kind == PanelInfoKind::Stack);
    utassertnear(state.nodes[state.center].sizes[0], 240.f);
    utassert(!AxisIsHorizontal(state.nodes[state.center].axis));
    const PanelStateNode& first =
        state.nodes[state.nodes[state.center].children[0]];
    utassert(first.kind == PanelInfoKind::Tabs);
    utassert(first.activeIndex == 1);
    utassert(StrEqI(state.nodes[first.children[0]].panelName,
                    StrL("AlphaPanel")));
    utassert(state.left.present && !state.left.open);
    utassertnear(state.left.size, 210.f);

    // Loaded back into a dock that knows the same panels: the same tree.
    DockState back;
    for (int i = 0; i < 3; i++) {
        DockPanelDef def;
        def.name = Str(kNames[i]);
        def.title = Str(kNames[i]);
        DockAddPanelDef(&back, def);
    }
    utassert(DockLoad(&back, &state, arena));
    utassert(back.nodes[back.center].split);
    utassert(back.nodes[back.center].child.len == 2);
    utassertnear(back.nodes[back.center].size[0], 240.f);
    int loadedTabs = back.nodes[back.center].child[0];
    utassert(back.nodes[loadedTabs].panel.len == 2);
    utassert(back.nodes[loadedTabs].panel[0] == 0);
    utassert(back.nodes[loadedTabs].activeIx == 1);
    utassert(back.left.node >= 0 && !back.left.open);
    utassertnear(back.left.size, 210.f);
    // Nothing new was registered: every name was one it already had.
    utassert(back.panels.len == 3);
    ArenaDelete(arena);
}

// PanelRegistry::build_panel answering with an InvalidPanel: the layout keeps
// its shape, and dumping it again writes the name it could not build.
static void APanelNothingAnswersToBecomesInvalid() {
    Arena* arena = ArenaNew();
    DockAreaState state;
    int tabs = state.NewNode(StrL("TabPanel"));
    state.nodes[tabs].kind = PanelInfoKind::Tabs;
    int known = state.NewNode(StrL("AlphaPanel"));
    int missing = state.NewNode(StrL("GitGraphPanel"));
    state.nodes[tabs].children.Append(known);
    state.nodes[tabs].children.Append(missing);
    state.nodes[tabs].activeIndex = 1;
    state.center = tabs;

    DockState s;
    DockPanelDef def;
    def.name = StrL("AlphaPanel");
    def.title = StrL("Alpha");
    DockAddPanelDef(&s, def);
    utassert(DockLoad(&s, &state, arena));
    // The missing one was registered on the spot, under the name asked for.
    utassert(s.panels.len == 2);
    utassert(StrEqI(s.panels[1].name, StrL("GitGraphPanel")));
    utassert(s.nodes[s.center].panel.len == 2);
    utassert(s.nodes[s.center].activeIx == 1);

    // Written out again, the layout still names it — Rust's InvalidPanel
    // dumps the state it came from rather than losing the panel.
    DockAreaState again;
    DockDump(&s, &again);
    const PanelStateNode& group = again.nodes[again.center];
    utassert(StrEqI(again.nodes[group.children[1]].panelName,
                    StrL("GitGraphPanel")));
    ArenaDelete(arena);
}

static void ALockedDockMovesNothing() {
    DockState s;
    int a = 0, b = 0;
    Seed(&s, &a, &b);
    s.locked = true;
    utassert(!DockMovePanelTo(&s, 1, b, DockDrop::Center));
    utassert(s.nodes[a].panel.len == 2);
}

void TestDock() {
    TheFiveDropZones();
    ThePlaceholderCoversEachZone();
    ADropInTheMiddleMerges();
    ADropOnAnEdgeSplits();
    AnEmptyGroupLeavesTheSplit();
    ARootGroupStays();
    ADropOnATabTakesItsPlace();
    AReorderCountsFromBeforeTheDetach();
    ADropOnItsOwnGroupNeedsATabOrAnEdge();
    ANodeKnowsWhichDockItIsIn();
    ATabOffTheEndIsBroughtIntoView();
    ALayoutSurvivesDumpAndLoad();
    APanelNothingAnswersToBecomesInvalid();
    ALockedDockMovesNothing();
}
