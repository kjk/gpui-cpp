#include "base/dock.h"

#include "base/resizable.h"

namespace gpui {

const Str kDockPanelDrag = StrL("dock-panel");
const Str kDockResizeDrag = StrL("dock-resize");

DockDrop DockDropAt(Bounds b, float x, float y) {
    if (x < b.x + b.w * 0.35f) {
        return DockDrop::Left;
    }
    if (x > b.x + b.w * 0.65f) {
        return DockDrop::Right;
    }
    if (y < b.y + b.h * 0.35f) {
        return DockDrop::Top;
    }
    if (y > b.y + b.h * 0.65f) {
        return DockDrop::Bottom;
    }
    return DockDrop::Center;
}

Bounds DockDropPlaceholder(Bounds b, DockDrop d) {
    float halfW = b.w * 0.5f;
    float halfH = b.h * 0.5f;
    switch (d) {
        case DockDrop::Left:
            return {b.x, b.y, halfW, b.h};
        case DockDrop::Right:
            return {b.x + halfW, b.y, halfW, b.h};
        case DockDrop::Top:
            return {b.x, b.y, b.w, halfH};
        case DockDrop::Bottom:
            return {b.x, b.y + halfH, b.w, halfH};
        default:
            return b;
    }
}

static void DockEmit(DockState* s, Ctx* cx) {
    DockEvent ev = {DockEventKind::LayoutChanged};
    if (s->onEvent.IsValid()) {
        ListenerCall(cx->app, cx->win, s->onEvent, &ev);
    }
    Notify(cx);
}

int DockPanelByName(const DockState* s, Str name) {
    if (!name.s || name.len <= 0) {
        return -1;
    }
    for (int i = 0; i < s->panels.len; i++) {
        if (StrSame(s->panels[i].name, name)) {
            return i;
        }
    }
    return -1;
}

int DockAddPanelDef(DockState* s, DockPanelDef def) {
    s->panels.Append(def);
    return s->panels.len - 1;
}

// A free slot, or a new one. The slot's own arrays are emptied rather than
// dropped: the pool owns them, and a node coming back into use starts with
// nothing in it.
static int DockNewNode(DockState* s) {
    for (int i = 0; i < s->nodes.len; i++) {
        if (!s->nodes[i].used) {
            DockNode& n = s->nodes[i];
            n.child.Clear();
            n.size.Clear();
            n.panel.Clear();
            n.split = false;
            n.axis = Axis::Horizontal;
            n.parent = -1;
            n.activeIx = 0;
            n.bounds = {};
            n.tabScrollX = 0;
            n.pendingScrollIx = -1;
            n.tabStripBounds = {};
            n.activeTabBounds = {};
            n.activeTabBoundsIx = -1;
            n.used = true;
            return i;
        }
    }
    s->nodes.Append(DockNode{});
    s->nodes[s->nodes.len - 1].used = true;
    return s->nodes.len - 1;
}

int DockNewTabs(DockState* s) {
    return DockNewNode(s);
}

int DockNewSplit(DockState* s, Axis axis) {
    int ix = DockNewNode(s);
    if (ix >= 0) {
        s->nodes[ix].split = true;
        s->nodes[ix].axis = axis;
    }
    return ix;
}

void DockTabsAdd(DockState* s, int node, int panelIx) {
    if (node < 0 || node >= s->nodes.len || panelIx < 0) {
        return;
    }
    DockNode& n = s->nodes[node];
    if (n.split) {
        return;
    }
    n.panel.Append(panelIx);
}

void DockTabsInsert(DockState* s, int node, int panelIx, int at) {
    if (node < 0 || node >= s->nodes.len || panelIx < 0) {
        return;
    }
    DockNode& n = s->nodes[node];
    if (n.split) {
        return;
    }
    if (at < 0 || at > n.panel.len) {
        at = n.panel.len;
    }
    n.panel.InsertAt(at, panelIx);
    // insert_panel_at ends with set_active_ix(ix): what was dropped is what
    // the group shows.
    n.activeIx = at;
    n.pendingScrollIx = at;
}

void DockSplitAdd(DockState* s, int node, int childNode, float size) {
    if (node < 0 || node >= s->nodes.len || childNode < 0) {
        return;
    }
    DockNode& n = s->nodes[node];
    if (!n.split) {
        return;
    }
    n.size.Append(size);
    n.child.Append(childNode);
    s->nodes[childNode].parent = node;
}

DockSide* DockSideOf(DockState* s, DockPlacement p) {
    switch (p) {
        case DockPlacement::Left:
            return &s->left;
        case DockPlacement::Bottom:
            return &s->bottom;
        case DockPlacement::Right:
            return &s->right;
        default:
            return nullptr;
    }
}

int DockNodeOfPanel(const DockState* s, int panelIx) {
    for (int i = 0; i < s->nodes.len; i++) {
        const DockNode& n = s->nodes[i];
        if (!n.used || n.split) {
            continue;
        }
        for (int j = 0; j < n.panel.len; j++) {
            if (n.panel[j] == panelIx) {
                return i;
            }
        }
    }
    return -1;
}

// Whichever root points at this node — the centre item or one of the three
// Docks — takes the replacement instead.
static void DockReplaceRoot(DockState* s, int oldNode, int newNode) {
    if (s->center == oldNode) {
        s->center = newNode;
    }
    if (s->left.node == oldNode) {
        s->left.node = newNode;
    }
    if (s->bottom.node == oldNode) {
        s->bottom.node = newNode;
    }
    if (s->right.node == oldNode) {
        s->right.node = newNode;
    }
}

// Put `newNode` where `oldNode` was, whether that is inside a split or at a
// root. The old node keeps what it holds; only its place changes.
static void DockReplace(DockState* s, int oldNode, int newNode) {
    int parent = s->nodes[oldNode].parent;
    s->nodes[newNode].parent = parent;
    if (parent < 0) {
        DockReplaceRoot(s, oldNode, newNode);
        return;
    }
    DockNode& p = s->nodes[parent];
    for (int i = 0; i < p.child.len; i++) {
        if (p.child[i] == oldNode) {
            p.child[i] = newNode;
            return;
        }
    }
}

static void DockRemoveNode(DockState* s, int node);

// remove_self_if_empty: a split down to one child is that child, and a tab
// group with nothing in it is gone. A root tab group stays — the Dock on a
// side always has one, which is why Rust makes its TabPanel `closable = false`.
static void DockPrune(DockState* s, int node) {
    if (node < 0 || !s->nodes[node].used) {
        return;
    }
    DockNode& n = s->nodes[node];
    if (n.split) {
        if (n.child.len == 1) {
            int only = n.child[0];
            DockReplace(s, node, only);
            n.used = false;
            return;
        }
        if (n.child.len == 0) {
            DockRemoveNode(s, node);
        }
        return;
    }
    if (n.panel.len == 0 && n.parent >= 0) {
        DockRemoveNode(s, node);
    }
}

// Take a node out of the split it is in, and hand its size to a neighbour.
static void DockRemoveNode(DockState* s, int node) {
    int parent = s->nodes[node].parent;
    s->nodes[node].used = false;
    if (parent < 0) {
        DockReplaceRoot(s, node, -1);
        return;
    }
    DockNode& p = s->nodes[parent];
    int at = -1;
    for (int i = 0; i < p.child.len; i++) {
        if (p.child[i] == node) {
            at = i;
            break;
        }
    }
    if (at < 0) {
        return;
    }
    int give = at > 0 ? at - 1 : (p.child.len > 1 ? at + 1 : -1);
    if (give >= 0) {
        p.size[give] += p.size[at];
    }
    for (int i = at; i < p.child.len - 1; i++) {
        p.child[i] = p.child[i + 1];
        p.size[i] = p.size[i + 1];
    }
    p.child.len--;
    p.size.len--;
    DockPrune(s, parent);
}

void DockSetActive(DockState* s, Ctx* cx, int node, int ix) {
    if (node < 0 || node >= s->nodes.len) {
        return;
    }
    DockNode& n = s->nodes[node];
    if (ix < 0 || ix >= n.panel.len) {
        return;
    }
    n.activeIx = ix;
    // pending_scroll_to_ix: the tab that was just made active is brought into
    // view on the next frame, where its box is known.
    n.pendingScrollIx = ix;
    Notify(cx);
}

// Take the panel out of its group without touching the tree; the callers
// decide what an empty group means.
static void DockDetach(DockState* s, int node, int ix) {
    DockNode& n = s->nodes[node];
    for (int i = ix; i < n.panel.len - 1; i++) {
        n.panel[i] = n.panel[i + 1];
    }
    n.panel.len--;
    if (n.activeIx >= n.panel.len) {
        n.activeIx = n.panel.len > 0 ? n.panel.len - 1 : 0;
    }
}

bool DockClosePanelAt(DockState* s, int node, int ix) {
    if (node < 0 || node >= s->nodes.len) {
        return false;
    }
    DockNode& n = s->nodes[node];
    if (ix < 0 || ix >= n.panel.len) {
        return false;
    }
    int panelIx = n.panel[ix];
    if (s->zoomPanel == panelIx) {
        s->zoomPanel = -1;
    }
    DockDetach(s, node, ix);
    DockPrune(s, node);
    return true;
}

void DockClosePanel(DockState* s, Ctx* cx, int node, int ix) {
    if (DockClosePanelAt(s, node, ix)) {
        DockEmit(s, cx);
    }
}

bool DockMovePanelTo(DockState* s, int panelIx, int to, DockDrop drop,
                     int atIx) {
    if (s->locked || to < 0 || to >= s->nodes.len || !s->nodes[to].used) {
        return false;
    }
    int from = DockNodeOfPanel(s, panelIx);
    if (from < 0) {
        return false;
    }
    // `is_same_tab && ix.is_none()`: a panel dropped back on its own group,
    // with no tab named to put it at, has nowhere to go. A tab named is a
    // reorder, which is a real move.
    if (from == to && drop == DockDrop::Center && atIx < 0) {
        return false;
    }
    // A group of one dropped on its own edge would split with itself, which
    // is the layout it already has.
    if (from == to && drop != DockDrop::Center &&
        s->nodes[from].panel.len <= 1) {
        return false;
    }
    int at = -1;
    for (int i = 0; i < s->nodes[from].panel.len; i++) {
        if (s->nodes[from].panel[i] == panelIx) {
            at = i;
            break;
        }
    }
    if (at < 0) {
        return false;
    }
    DockDetach(s, from, at);

    if (drop == DockDrop::Center) {
        if (atIx >= 0) {
            // Rust inserts at the index the drop named, worked out before the
            // panel was detached — so dragging a tab rightwards inside its own
            // group lands it one place short of where it was let go, and this
            // does the same.
            DockTabsInsert(s, to, panelIx, atIx);
        } else {
            DockTabsAdd(s, to, panelIx);
            s->nodes[to].activeIx = s->nodes[to].panel.len - 1;
        }
        DockPrune(s, from);
        return true;
    }

    int fresh = DockNewTabs(s);
    if (fresh < 0) {
        // Nowhere to put it: give it back rather than lose it.
        DockTabsAdd(s, from, panelIx);
        return false;
    }
    DockTabsAdd(s, fresh, panelIx);

    bool horizontal = drop == DockDrop::Left || drop == DockDrop::Right;
    bool before = drop == DockDrop::Left || drop == DockDrop::Top;
    Axis axis = horizontal ? Axis::Horizontal : Axis::Vertical;

    int parent = s->nodes[to].parent;
    if (parent >= 0 && s->nodes[parent].axis == axis && true) {
        // The split already runs this way, so the new group joins it beside
        // the target and the two share what the target had.
        DockNode& p = s->nodes[parent];
        int ix = 0;
        for (int i = 0; i < p.child.len; i++) {
            if (p.child[i] == to) {
                ix = i;
                break;
            }
        }
        int insert = before ? ix : ix + 1;
        float half = p.size[ix] * 0.5f;
        p.child.Append(0);
        p.size.Append(0);
        for (int i = p.child.len - 1; i > insert; i--) {
            p.child[i] = p.child[i - 1];
            p.size[i] = p.size[i - 1];
        }
        p.child[insert] = fresh;
        p.size[insert] = half;
        p.size[before ? ix + 1 : ix] = half;

        s->nodes[fresh].parent = parent;
    } else {
        int split = DockNewSplit(s, axis);
        if (split < 0) {
            DockTabsAdd(s, from, panelIx);
            s->nodes[fresh].used = false;
            return false;
        }
        DockReplace(s, to, split);
        float whole =
            horizontal ? s->nodes[to].bounds.w : s->nodes[to].bounds.h;
        if (whole < kDockPanelMinSize * 2) {
            whole = kDockPanelMinSize * 2;
        }
        float half = whole * 0.5f;
        if (before) {
            DockSplitAdd(s, split, fresh, half);
            DockSplitAdd(s, split, to, half);
        } else {
            DockSplitAdd(s, split, to, half);
            DockSplitAdd(s, split, fresh, half);
        }
    }
    DockPrune(s, from);
    return true;
}

void DockMovePanel(DockState* s, Ctx* cx, int panelIx, int to, DockDrop drop,
                   int atIx) {
    if (DockMovePanelTo(s, panelIx, to, drop, atIx)) {
        DockEmit(s, cx);
    }
}

float DockTabScrollTo(float scrollX, Bounds strip, Bounds tab) {
    if (strip.w <= 0 || tab.w <= 0) {
        return scrollX;
    }
    // The boxes are where they were painted, so what is asked for is the
    // change to the offset rather than the offset itself.
    if (tab.x < strip.x) {
        return scrollX - (strip.x - tab.x);
    }
    float tabRight = tab.x + tab.w;
    float stripRight = strip.x + strip.w;
    if (tabRight > stripRight) {
        return scrollX + (tabRight - stripRight);
    }
    return scrollX;
}

DockPlacement DockPlacementOfNode(const DockState* s, int node) {
    if (node < 0 || node >= s->nodes.len) {
        return DockPlacement::Center;
    }
    // Up to the root of whichever tree it is in, and then which one that was.
    int root = node;
    for (int guard = 0; guard < s->nodes.len + 1; guard++) {
        int parent = s->nodes[root].parent;
        if (parent < 0) {
            break;
        }
        root = parent;
    }
    if (root == s->left.node) {
        return DockPlacement::Left;
    }
    if (root == s->right.node) {
        return DockPlacement::Right;
    }
    if (root == s->bottom.node) {
        return DockPlacement::Bottom;
    }
    return DockPlacement::Center;
}

int DockVisibleCount(const DockState* s, int node) {
    if (node < 0 || node >= s->nodes.len || !s->nodes[node].used) {
        return 0;
    }
    const DockNode& n = s->nodes[node];
    int count = 0;
    for (int i = 0; i < n.panel.len; i++) {
        if (s->panels[n.panel[i]].visible) {
            count++;
        }
    }
    return count;
}

int DockActiveIx(const DockState* s, int node) {
    if (node < 0 || node >= s->nodes.len || !s->nodes[node].used) {
        return -1;
    }
    const DockNode& n = s->nodes[node];
    if (n.activeIx >= 0 && n.activeIx < n.panel.len &&
        s->panels[n.panel[n.activeIx]].visible) {
        return n.activeIx;
    }
    // `active_panel`: a hidden panel sitting at the active index is stood in
    // for by the first visible one rather than showing nothing.
    for (int i = 0; i < n.panel.len; i++) {
        if (s->panels[n.panel[i]].visible) {
            return i;
        }
    }
    return -1;
}

bool DockNodeLocked(const DockState* s, int node) {
    if (node < 0 || node >= s->nodes.len || !s->nodes[node].used) {
        return true;
    }
    if (s->locked) {
        return true;
    }
    // `self.zoomed`: the group holding the zoomed panel is the whole area,
    // and nothing may be dragged out of it until it is zoomed back.
    if (s->zoomPanel >= 0 && DockNodeOfPanel(s, s->zoomPanel) == node) {
        return true;
    }
    // `self.stack_panel.is_none()`: a group with no split above it is the
    // root of its tree and cannot be taken apart.
    return s->nodes[node].parent < 0;
}

bool DockIsLastPanel(const DockState* s, int node) {
    if (node < 0 || node >= s->nodes.len || !s->nodes[node].used) {
        return true;
    }
    // StackPanel::is_last_panel, walked up: a split holding more than one
    // child means there is somewhere for this panel to go.
    for (int at = s->nodes[node].parent, guard = 0;
         at >= 0 && guard < s->nodes.len + 1; at = s->nodes[at].parent) {
        guard++;
        if (s->nodes[at].child.len > 1) {
            return false;
        }
    }
    return DockVisibleCount(s, node) <= 1;
}

bool DockNodeDraggable(const DockState* s, int node) {
    return !DockNodeLocked(s, node) && !DockIsLastPanel(s, node);
}

bool DockNodeDroppable(const DockState* s, int node) {
    return !DockNodeLocked(s, node);
}

int DockLeftTopTabs(const DockState* s, int node) {
    if (node < 0 || node >= s->nodes.len || !s->nodes[node].used) {
        return -1;
    }
    if (!s->nodes[node].split) {
        return node;
    }
    if (s->nodes[node].child.len == 0) {
        return -1;
    }
    return DockLeftTopTabs(s, s->nodes[node].child[0]);
}

int DockRightTopTabs(const DockState* s, int node) {
    if (node < 0 || node >= s->nodes.len || !s->nodes[node].used) {
        return -1;
    }
    const DockNode& n = s->nodes[node];
    if (!n.split) {
        return node;
    }
    if (n.child.len == 0) {
        return -1;
    }
    // A split down the page keeps its first child — the top one — and a split
    // across the page its last, which is the right-most.
    int pick = AxisIsHorizontal(n.axis) ? n.child[n.child.len - 1] : n.child[0];
    return DockRightTopTabs(s, pick);
}

void DockSetCollapsible(DockState* s, DockPlacement p, bool collapsible) {
    DockSide* side = DockSideOf(s, p);
    if (!side) {
        return;
    }
    side->collapsible = collapsible;
    // Dock::set_collapsible: a dock nobody can collapse is opened here and
    // now, or a dock shut before the flag was cleared could never be reached.
    if (!collapsible) {
        side->open = true;
    }
}

void DockToggleSide(DockState* s, Ctx* cx, DockPlacement p) {
    DockSide* side = DockSideOf(s, p);
    if (!side || !side->collapsible) {
        return;
    }
    side->open = !side->open;
    DockEmit(s, cx);
}

void DockResizeSide(DockState* s, Ctx* cx, DockPlacement p, float x, float y) {
    DockSide* side = DockSideOf(s, p);
    if (!side) {
        return;
    }
    if (!side->open) {
        side->open = true;
    }
    Bounds b = s->bounds;
    float leftSize =
        (p != DockPlacement::Left && s->left.node >= 0 && s->left.open)
            ? s->left.size
            : 0;
    float rightSize =
        (p != DockPlacement::Right && s->right.node >= 0 && s->right.open)
            ? s->right.size
            : 0;
    float size = 0;
    float maxSize = 0;
    switch (p) {
        case DockPlacement::Left:
            size = x - b.x;
            maxSize = b.w - kDockPanelMinSize - rightSize;
            break;
        case DockPlacement::Right:
            size = b.Right() - x;
            maxSize = b.w - kDockPanelMinSize - leftSize;
            break;
        case DockPlacement::Bottom:
            size = b.Bottom() - y;
            maxSize = b.h - kDockPanelMinSize;
            break;
        default:
            return;
    }
    if (maxSize < kDockPanelMinSize) {
        maxSize = kDockPanelMinSize;
    }
    if (size < kDockPanelMinSize) {
        size = kDockPanelMinSize;
    }
    if (size > maxSize) {
        size = maxSize;
    }
    side->size = size;
    Notify(cx);
}

void DockToggleZoom(DockState* s, Ctx* cx, int panelIx) {
    s->zoomPanel = s->zoomPanel == panelIx ? -1 : panelIx;
    Notify(cx);
}

void DockState::OnTabClick(DockState* self, Ctx* cx, const ClickEvent*,
                           intptr_t nodeAndIx) {
    int node = DockUnpackNode(nodeAndIx);
    DockSetActive(self, cx, node, DockUnpackIx(nodeAndIx));
    // "Open dock if clicked on the collapsed bottom dock": its tab bar is all
    // that is left of it, so a click there is what opens it again.
    DockPlacement p = DockPlacementOfNode(self, node);
    DockSide* side = DockSideOf(self, p);
    if (side && !side->open) {
        DockToggleSide(self, cx, p);
    }
}

void DockState::OnCloseClick(DockState* self, Ctx* cx, const ClickEvent*,
                             intptr_t nodeAndIx) {
    DockClosePanel(self, cx, DockUnpackNode(nodeAndIx),
                   DockUnpackIx(nodeAndIx));
}

void DockState::OnZoomClick(DockState* self, Ctx* cx, const ClickEvent*,
                            intptr_t panelIx) {
    DockToggleZoom(self, cx, (int)panelIx);
}

void DockState::OnToggleSide(DockState* self, Ctx* cx, const ClickEvent*,
                             intptr_t placement) {
    DockToggleSide(self, cx, (DockPlacement)placement);
}

void DockState::OnTabDragMove(DockState* self, Ctx* cx,
                              const DragMoveEvent* ev) {
    if (!StrSame(ev->drag.kind, kDockPanelDrag) || self->locked) {
        return;
    }
    // The tab group under the pointer, and which of its five zones. Last
    // frame's boxes are what there is to ask; the ones Rust asks are just as
    // old.
    int found = -1;
    for (int i = 0; i < self->nodes.len; i++) {
        const DockNode& n = self->nodes[i];
        if (!n.used || n.split) {
            continue;
        }
        if (n.bounds.Contains({ev->event.x, ev->event.y})) {
            found = i;
            break;
        }
    }
    DockDrop at = DockDrop::Center;
    if (found >= 0) {
        at = DockDropAt(self->nodes[found].bounds, ev->event.x, ev->event.y);
    }
    if (self->dropNode == found && self->dropAt == at) {
        return;
    }
    // A drag arriving at a group brings the placeholder in from the dragged
    // tab's preview; moving between the zones of one carries on from where
    // the placeholder already is, which is what the transition does anyway.
    if (self->dropNode != found && found >= 0 && cx->win) {
        Point off = WindowDragOffset(cx);
        self->dropFrom = {cx->win->mouseX - off.x, cx->win->mouseY - off.y,
                          kDockDragPreviewW, kDockDragPreviewH};
        self->dropFromPending = true;
    }
    self->dropNode = found;
    self->dropAt = at;
    Notify(cx);
}

void DockState::OnTabDragEnd(DockState* self, Ctx* cx, const MouseUpEvent*) {
    if (self->dropNode < 0) {
        return;
    }
    self->dropNode = -1;
    Notify(cx);
}

void DockState::OnDropPanel(DockState* self, Ctx* cx, const DropEvent* ev,
                            intptr_t node) {
    if (!StrSame(ev->drag.kind, kDockPanelDrag)) {
        return;
    }
    DockDrop at = DockDropAt(ev->el, ev->x, ev->y);
    self->dropNode = -1;
    DockMovePanel(self, cx, ev->drag.ix, (int)node, at);
}

void DockState::OnDropTab(DockState* self, Ctx* cx, const DropEvent* ev,
                          intptr_t nodeAndIx) {
    if (!StrSame(ev->drag.kind, kDockPanelDrag)) {
        return;
    }
    // `will_split_placement = None`: a drop on a tab is always a move into
    // that row, whatever zone of the body the pointer had been over.
    self->dropNode = -1;
    DockMovePanel(self, cx, ev->drag.ix, DockUnpackNode(nodeAndIx),
                  DockDrop::Center, DockUnpackIx(nodeAndIx));
}

void DockState::OnDropTabBar(DockState* self, Ctx* cx, const DropEvent* ev,
                             intptr_t node) {
    if (!StrSame(ev->drag.kind, kDockPanelDrag)) {
        return;
    }
    self->dropNode = -1;
    int to = (int)node;
    // The empty space past the last tab. Rust names the last index when the
    // panel is already in this row — which is what moves a tab to the end —
    // and leaves it unnamed otherwise, so the panel is appended.
    int at = -1;
    if (DockNodeOfPanel(self, ev->drag.ix) == to) {
        at = self->nodes[to].panel.len - 1;
    }
    DockMovePanel(self, cx, ev->drag.ix, to, DockDrop::Center, at);
}

void DockState::OnMenuItem(DockState* self, Ctx* cx, const ClickEvent*,
                           intptr_t item) {
    // The menu reports which of its rows was taken; which panel that is about
    // is the group whose menu was open when it did.
    int node = self->menuNode;
    if (node < 0 || node >= self->nodes.len || !self->nodes[node].used) {
        return;
    }
    DockNode& n = self->nodes[node];
    if (n.panel.len <= 0) {
        return;
    }
    // Row 0 is ToggleZoom; the separator is row 1, so anything past it is
    // ClosePanel on the active one.
    if (item == 0) {
        DockToggleZoom(self, cx, n.panel[n.activeIx]);
    } else {
        DockClosePanel(self, cx, node, n.activeIx);
    }
}

void DockState::OnTabBarScroll(DockState* self, Ctx* cx, const ScrollEvent* ev,
                               intptr_t nodeArg) {
    int node = (int)nodeArg;
    if (node < 0 || node >= self->nodes.len || !self->nodes[node].used) {
        return;
    }
    self->nodes[node].tabScrollX = ev->offsetX;
    Notify(cx);
}

void DockState::OnResizeDrag(DockState* self, Ctx* cx,
                             const DragMoveEvent* ev) {
    if (!StrSame(ev->drag.kind, kDockResizeDrag)) {
        return;
    }
    self->resizing = true;
    int node = DockUnpackNode(ev->drag.ix);
    // A node index past the end of the tree is one of the three Docks, which
    // resize against the whole area rather than against a split's children.
    if (node >= kDockSideBase) {
        DockPlacement p = (DockPlacement)(node - kDockSideBase);
        self->resizingSide = p;
        DockResizeSide(self, cx, p, ev->event.x, ev->event.y);
        return;
    }
    if (node < 0 || !self->nodes[node].used || !self->nodes[node].split) {
        return;
    }
    DockNode& n = self->nodes[node];
    int ix = DockUnpackIx(ev->drag.ix);
    bool horizontal = AxisIsHorizontal(n.axis);
    float container = horizontal ? n.bounds.w : n.bounds.h;
    ResizableAdjustToContainer(n.size.els, n.child.len, container);
    float start = horizontal ? n.bounds.x : n.bounds.y;
    for (int i = 0; i < ix; i++) {
        start += n.size[i];
    }
    float want = (horizontal ? ev->event.x : ev->event.y) - start;
    ResizablePanelResize(n.size.els, nullptr, nullptr, n.child.len, ix, want,
                         container);
    Notify(cx);
}

void DockState::OnResizeEnd(DockState* self, Ctx* cx, const MouseUpEvent*) {
    if (!self->resizing) {
        return;
    }
    self->resizing = false;
    self->resizingSide = DockPlacement::Center;
    DockEmit(self, cx);
}

} // namespace gpui
