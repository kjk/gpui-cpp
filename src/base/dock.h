/* Unstyled dock — crates/ui/src/dock

   The dock is a tree: a DockItem is either a group of tabs or a split of
   other items, and a DockArea holds one of those in its centre plus a fixed
   Dock on the left, the right and the bottom. Rust builds the tree out of
   entities holding `Arc<dyn PanelView>`; there is no dyn here, so a panel is
   a title and a function that renders it, and the tree is an array of nodes
   naming each other by index. */

#include "gpui/gpui.h"

namespace gpui {

// PANEL_MIN_SIZE, from crates/base/src/resizable.
const float kDockPanelMinSize = 100.f;
// A closed bottom dock keeps its tab bar, so there is something left to click
// to open it again (Dock::render).
const float kDockCollapsedH = 29.f;

// What a press on a tab picks up, and what a press on a resize handle does.
// `ix` is the panel for the first and the handle for the second.
extern const Str kDockPanelDrag;
extern const Str kDockResizeDrag;

// The four places a Dock can be. Center is the DockArea's own item, which is
// not a Dock and never closes.
enum class DockPlacement : uint8_t {
    Center,
    Left,
    Bottom,
    Right
};

// split_placement_at: an edge means split the target that way, the middle
// means merge into its tab group.
enum class DockDrop : uint8_t {
    Center,
    Left,
    Right,
    Top,
    Bottom
};

// Which of the five zones of `b` the position is in. Rust's thresholds are
// 35% and 65% of each side.
DockDrop DockDropAt(Bounds b, float x, float y);
// DropPlaceholderBounds::for_placement: the half of `b` a drop would land in,
// or all of it for a merge.
Bounds DockDropPlaceholder(Bounds b, DockDrop d);

const int kMaxDockPanels = 32;
const int kMaxDockNodes = 32;
const int kMaxDockChildren = 8;

// One panel the host handed the dock. Rust's `Arc<dyn PanelView>` is a title,
// a render and the two questions the tab bar asks it.
struct DockPanelDef {
    Str title = {};
    El* (*render)(Ctx* cx, void* data) = nullptr;
    void* data = nullptr;
    bool closable = true;
    bool zoomable = true;
};

// DockItem. A node is either Tabs — a list of panels with one active — or
// Split — a list of child nodes along an axis, each with a size.
struct DockNode {
    bool used = false;
    bool split = false;
    Axis axis = Axis::Horizontal;
    int parent = -1;
    int child[kMaxDockChildren] = {};
    float size[kMaxDockChildren] = {};
    int nChild = 0;
    int panel[kMaxDockPanels] = {};
    int nPanel = 0;
    int activeIx = 0;
    // Where the node was last painted. The drop zone under the pointer can
    // only be worked out against boxes, and the boxes are last frame's.
    Bounds bounds = {};
};

// A Dock: the fixed container on one side of the area.
struct DockSide {
    int node = -1;
    bool open = true;
    bool collapsible = true;
    float size = 200;
};

enum class DockEventKind : uint8_t {
    // DockEvent::LayoutChanged.
    LayoutChanged
};

struct DockEvent {
    DockEventKind kind = DockEventKind::LayoutChanged;
};

struct DockState {
    DockPanelDef panels[kMaxDockPanels] = {};
    int nPanels = 0;
    DockNode nodes[kMaxDockNodes] = {};
    int center = -1;
    DockSide left = {};
    DockSide bottom = {};
    DockSide right = {};
    // ToggleZoom: the one panel filling the whole area, or -1.
    int zoomPanel = -1;
    // DockArea::locked. A locked dock still resizes; it just does not move
    // panels around.
    bool locked = false;
    // DockArea::bounds, written at paint.
    Bounds bounds = {};
    // Which node a drag is over and where it would land, so the placeholder
    // can be drawn before the button comes up. -1 when no drag is in flight.
    int dropNode = -1;
    DockDrop dropAt = DockDrop::Center;
    // Which group's ⋯ menu is open, so the row it reports lands on the right
    // panel. Rust's menu is built by the TabPanel itself and closes over it;
    // a menu here reports only which row was taken.
    int menuNode = -1;
    // The side being resized, and the split handle being dragged.
    DockPlacement resizingSide = DockPlacement::Center;
    bool resizing = false;

    Listener onEvent;

    static void OnTabClick(DockState* self, Ctx* cx, const ClickEvent* ev,
                           intptr_t nodeAndIx);
    static void OnCloseClick(DockState* self, Ctx* cx, const ClickEvent* ev,
                             intptr_t nodeAndIx);
    static void OnZoomClick(DockState* self, Ctx* cx, const ClickEvent* ev,
                            intptr_t panelIx);
    static void OnToggleSide(DockState* self, Ctx* cx, const ClickEvent* ev,
                             intptr_t placement);
    static void OnTabDragMove(DockState* self, Ctx* cx,
                              const DragMoveEvent* ev);
    static void OnTabDragEnd(DockState* self, Ctx* cx, const MouseUpEvent* ev);
    static void OnDropPanel(DockState* self, Ctx* cx, const DropEvent* ev,
                            intptr_t node);
    // A drop on a tab, which names the place in the row the panel takes, and
    // one on the empty space past the last tab, which appends. Rust's two
    // `on_drop` closures on the tab bar.
    static void OnDropTab(DockState* self, Ctx* cx, const DropEvent* ev,
                          intptr_t nodeAndIx);
    static void OnDropTabBar(DockState* self, Ctx* cx, const DropEvent* ev,
                             intptr_t node);
    // The ⋯ menu: Zoom In / Zoom Out and Close, over the active panel.
    static void OnMenuItem(DockState* self, Ctx* cx, const ClickEvent* ev,
                           intptr_t nodeAndIx);
    static void OnResizeDrag(DockState* self, Ctx* cx, const DragMoveEvent* ev);
    static void OnResizeEnd(DockState* self, Ctx* cx, const MouseUpEvent* ev);
};

// A node index and a slot inside it, packed into the one intptr_t a listener
// carries. Rust's closure captures both outright.
inline intptr_t DockPack(int node, int ix) {
    return (intptr_t)(node * 64 + ix);
}
inline int DockUnpackNode(intptr_t v) {
    return (int)(v / 64);
}
inline int DockUnpackIx(intptr_t v) {
    return (int)(v % 64);
}

// Register a panel. The index it answers with is what the tree names.
int DockAddPanelDef(DockState* s, DockPanelDef def);
// DockItem::tabs / DockItem::split, as node indices. -1 when the tree is full.
int DockNewTabs(DockState* s);
int DockNewSplit(DockState* s, Axis axis);
// Add a panel to a tab group, or a child to a split. A child's size is its
// extent along the split's axis.
void DockTabsAdd(DockState* s, int node, int panelIx);
// insert_panel_at: the same, at a place in the row rather than at the end,
// and the inserted panel becomes the active one.
void DockTabsInsert(DockState* s, int node, int panelIx, int at);
void DockSplitAdd(DockState* s, int node, int childNode, float size);
// The active tab of a group.
void DockSetActive(DockState* s, Ctx* cx, int node, int ix);
// Panel::closable: take the panel out of its group. An empty group leaves the
// split it was in, and a split with one child left is replaced by that child,
// which is what Rust's remove_self_if_empty does.
void DockClosePanel(DockState* s, Ctx* cx, int node, int ix);
// The drag landed: merge into `to`'s tabs, or split `to` that way. Moving a
// panel onto its own group is a no-op the way Rust's is.
//
// `atIx` is Rust's `ix: Option<usize>` — the tab the drop landed on, so the
// panel takes that place in the row instead of the end. -1 is `None`: the
// drop was on the body, or on the empty space past the last tab. A drop onto
// a tab of the group the panel is already in is a reorder, which is the one
// case a same-group drop is not a no-op.
void DockMovePanel(DockState* s, Ctx* cx, int panelIx, int to, DockDrop drop,
                   int atIx = -1);
// The tree half of those two, with no window to notify: the listeners call
// these and then emit, and a test drives them on their own.
bool DockClosePanelAt(DockState* s, int node, int ix);
bool DockMovePanelTo(DockState* s, int panelIx, int to, DockDrop drop,
                     int atIx = -1);
// Dock::toggle_open, and the size a drag on its edge asks for.
void DockToggleSide(DockState* s, Ctx* cx, DockPlacement p);
void DockResizeSide(DockState* s, Ctx* cx, DockPlacement p, float x, float y);
// ToggleZoom.
void DockToggleZoom(DockState* s, Ctx* cx, int panelIx);
// The Dock on one side, or null for Center.
DockSide* DockSideOf(DockState* s, DockPlacement p);
// Which tab group holds this panel, or -1.
int DockNodeOfPanel(const DockState* s, int panelIx);
// Which Dock a node is in — the side whose tree it hangs under, or Center for
// the area's own item. What a click on a collapsed dock's tab needs to know.
DockPlacement DockPlacementOfNode(const DockState* s, int node);

} // namespace gpui
