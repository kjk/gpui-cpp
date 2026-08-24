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
// DragPanel's own box — `w_24` and a line of text between two paddings —
// which is both what follows the pointer and where the drop placeholder flies
// in from.
const float kDockDragPreviewW = 96.f;
const float kDockDragPreviewH = 30.f;

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

// A node index past the pool, which is what a listener bound to one of the
// three docks rather than to a node carries. The pool grows, so this is a
// number no node index can reach rather than the pool's own size.
const int kDockSideBase = 1 << 20;

// Panel::zoomable, which is an `Option<PanelControl>` in Rust and defaults to
// `Some(Menu)`: where the zoom action shows up. A panel that cannot be zoomed
// at all is `No`, and the maximise icon is on the bar only for Toolbar and
// Both — every other panel offers Zoom In in the ⋯ menu and nowhere else.
enum class DockPanelControl : uint8_t {
    No,
    Menu,
    Toolbar,
    Both
};

inline bool DockPanelControlToolbar(DockPanelControl c) {
    return c == DockPanelControl::Toolbar || c == DockPanelControl::Both;
}
inline bool DockPanelControlMenu(DockPanelControl c) {
    return c == DockPanelControl::Menu || c == DockPanelControl::Both;
}

// DockArea::panel_style. Auto is Rust's default: a group showing one panel is
// a plain title row rather than a tab bar, and only a second tab turns the bar
// on. TabBar is the opt-out that always shows tabs.
enum class DockPanelStyle : uint8_t {
    Auto,
    TabBar
};

// One panel the host handed the dock. Rust's `Arc<dyn PanelView>` is a title,
// a render and the two questions the tab bar asks it.
struct DockPanelDef {
    // Panel::panel_name(): what a saved layout stores and what the registry
    // is asked for on the way back. Not the title — that is what the tab
    // shows, and it can change while the name cannot.
    Str name = {};
    Str title = {};
    El* (*render)(Ctx* cx, void* data) = nullptr;
    // Panel::title_suffix: what the panel puts in its own tab bar, between
    // the tabs and the group's zoom and menu buttons. A panel that wants a
    // search box or a status of its own has nowhere else to put it.
    El* (*titleSuffix)(Ctx* cx, void* data) = nullptr;
    // Panel::tab_name(): the short label a tab shows when the title is too
    // much for one. Empty falls back to the title, and the single-panel title
    // row always shows the title.
    Str tabName = {};
    void* data = nullptr;
    bool closable = true;
    // Panel::visible(). A hidden panel has no tab, is not the active panel,
    // and does not count towards the one that may not be closed or dragged.
    bool visible = true;
    DockPanelControl zoomable = DockPanelControl::Menu;
};

// DockItem. A node is either Tabs — a list of panels with one active — or
// Split — a list of child nodes along an axis, each with a size.
struct DockNode {
    DockNode() = default;

    bool used = false;
    bool split = false;
    Axis axis = Axis::Horizontal;
    int parent = -1;
    // A split's children and their sizes, and a tab group's panels: as many
    // of either as the caller builds. Rust holds a Vec for each.
    Vec<int> child;
    Vec<float> size;
    Vec<int> panel;
    int activeIx = 0;
    // Where the node was last painted. The drop zone under the pointer can
    // only be worked out against boxes, and the boxes are last frame's.
    Bounds bounds = {};
    // The tab bar's own scroll, for a row of tabs wider than the bar, and the
    // tab it has been asked to bring into view — Rust's `tab_bar_scroll_handle`
    // and `pending_scroll_to_ix`. The two boxes are last frame's, which is
    // what the offset is worked out from.
    float tabScrollX = 0;
    int pendingScrollIx = -1;
    Bounds tabStripBounds = {};
    Bounds activeTabBounds = {};
    // Which tab `activeTabBounds` was measured for. A tab just made active
    // has no box yet — the frame that shows it is the one that measures it —
    // so the scroll waits for the frame after that.
    int activeTabBoundsIx = -1;
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
    Vec<DockPanelDef> panels;
    // The node pool. A free slot is one whose `used` is false, and the pool
    // grows when there is none — `DockNode` holds arrays of its own, so a
    // slot is emptied rather than dropped.
    Vec<DockNode> nodes;
    int center = -1;
    DockSide left = {};
    DockSide bottom = {};
    DockSide right = {};
    // ToggleZoom: the one panel filling the whole area, or -1.
    int zoomPanel = -1;
    // DockArea::locked. A locked dock still resizes; it just does not move
    // panels around.
    bool locked = false;
    // DockArea::panel_style, which every group in the area reads.
    DockPanelStyle panelStyle = DockPanelStyle::Auto;
    // DockArea::toggle_button_visible: whether the three dock toggles are
    // drawn at all.
    bool toggleButtonVisible = true;
    // DockArea::version, kept so a layout that was loaded writes back the
    // version it came with.
    bool hasVersion = false;
    int version = 0;
    // DockArea::bounds, written at paint.
    Bounds bounds = {};
    // Which node a drag is over and where it would land, so the placeholder
    // can be drawn before the button comes up. -1 when no drag is in flight.
    int dropNode = -1;
    DockDrop dropAt = DockDrop::Center;
    // Where the placeholder flies in from when a drag first reaches a group:
    // the dragged tab's own preview, which is what Rust's `source` is for a
    // panel drag. Pending until the frame that draws it has seeded the
    // transition with it.
    Bounds dropFrom = {};
    bool dropFromPending = false;
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
    // The tab bar scrolled sideways, which a row of tabs too wide for it does.
    static void OnTabBarScroll(DockState* self, Ctx* cx, const ScrollEvent* ev,
                               intptr_t node);
    static void OnResizeDrag(DockState* self, Ctx* cx, const DragMoveEvent* ev);
    static void OnResizeEnd(DockState* self, Ctx* cx, const MouseUpEvent* ev);

    ~DockState() {
        for (int i = 0; i < nodes.len; i++) {
            nodes[i].child.Reset();
            nodes[i].size.Reset();
            nodes[i].panel.Reset();
        }
        nodes.Reset();
        panels.Reset();
    }
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
// ScrollHandle::scroll_to_item: the sideways offset that brings `tab` inside
// `strip`, from where the strip is scrolled now. A tab already inside asks for
// nothing; one off either end is brought just inside that edge.
float DockTabScrollTo(float scrollX, Bounds strip, Bounds tab);

// Dock::toggle_open, and the size a drag on its edge asks for.
void DockToggleSide(DockState* s, Ctx* cx, DockPlacement p);
void DockResizeSide(DockState* s, Ctx* cx, DockPlacement p, float x, float y);
// ToggleZoom.
void DockToggleZoom(DockState* s, Ctx* cx, int panelIx);
// The Dock on one side, or null for Center.
DockSide* DockSideOf(DockState* s, DockPlacement p);
// Which tab group holds this panel, or -1.
int DockNodeOfPanel(const DockState* s, int panelIx);
// PanelRegistry::build_panel's lookup half: the panel registered under this
// name, or -1.
int DockPanelByName(const DockState* s, Str name);
// Which Dock a node is in — the side whose tree it hangs under, or Center for
// the area's own item. What a click on a collapsed dock's tab needs to know.
DockPlacement DockPlacementOfNode(const DockState* s, int node);

// Panel::visible: how many of a group's panels are shown, and the index of
// the one that is active — a hidden panel sitting at `activeIx` falls back to
// the first visible one, which is Rust's `active_panel`.
int DockVisibleCount(const DockState* s, int node);
int DockActiveIx(const DockState* s, int node);
// TabPanel::is_locked: the area is locked, this group is the zoomed one, or
// it is a root with no split above it.
bool DockNodeLocked(const DockState* s, int node);
// TabPanel::is_last_panel: this group shows one panel at most and no split
// above it holds more than one child. The last panel of a dock is neither
// draggable nor closable, so a dock can never be emptied by hand.
bool DockIsLastPanel(const DockState* s, int node);
// draggable() / droppable(): what the two guards above come to.
bool DockNodeDraggable(const DockState* s, int node);
bool DockNodeDroppable(const DockState* s, int node);
// StackPanel::left_top_tab_panel / right_top_tab_panel: which group carries
// each of the three dock toggle buttons. The right one follows the split's
// axis — the last child across, the first one down — and the bottom toggle
// belongs to the bottom Dock's own first group.
int DockLeftTopTabs(const DockState* s, int node);
int DockRightTopTabs(const DockState* s, int node);
// Dock::set_collapsible: a dock that may not be collapsed is opened, so it
// can never be left shut and unreachable.
void DockSetCollapsible(DockState* s, DockPlacement p, bool collapsible);

} // namespace gpui
