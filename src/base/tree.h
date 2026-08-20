/* Unstyled tree — crates/base/src/tree.rs */

#include "gpui/gpui.h"
#include "base/virtual_list.h"

namespace gpui {

// What a keystroke asks a tree to do. Rust binds up, down, left and right in
// the tree's key context; left and right are the folder pair, and each only
// acts in one direction. Enter is Confirm, which toggles a folder.
enum class TreeAction : uint8_t {
    None,
    SelectPrev,
    SelectNext,
    Collapse,
    Expand,
    Confirm
};

// tree.rs::init: up, down, left and right in the "Tree" key context, plus
// the enter its on_action_confirm answers to.
void TreeInitKeys();
Str TreeContext();
TreeAction TreeActionOf(uint32_t id);

// Where Up and Down move the selection. Both wrap, and both treat an unset
// selection as 0 before stepping — which is why Up from nothing lands on the
// last entry while Down from nothing lands on the second. Rust's
// `checked_sub(1).unwrap_or(len - 1)` against its `if ix + 1 < len`.
// `selected` is -1 for unset.
int TreeSelectPrev(int selected, int count);
int TreeSelectNext(int selected, int count);

// Left collapses an expanded folder and does nothing else; Right expands a
// collapsed one. Neither touches a leaf, and neither is a toggle.
bool TreeCollapses(bool isFolder, bool isExpanded);
bool TreeExpands(bool isFolder, bool isExpanded);

const int kMaxTreeItems = 512;

// TreeItem. Rust's holds its children and shares its expanded flag through an
// Rc; the items here live in one array on the state and name their parent,
// which is the same tree without the reference counting.
struct TreeItem {
    Str id = {};
    Str label = {};
    int parent = -1;
    int depth = 0;
    // Whether anything named this item as its parent. Rust asks the children
    // vector, which is the same question.
    bool folder = false;
    bool expanded = false;
    bool disabled = false;
};

enum class TreeEventKind : uint8_t {
    Expanded,
    Collapsed
};

struct TreeEvent {
    TreeEventKind kind = TreeEventKind::Expanded;
    // The item the event is about: its id, as Rust's variants carry, and
    // where it is in the flattened list.
    Str id = {};
    int ix = 0;
};

// TreeState. The items, the flattened list of the ones on screen, and what a
// click or a key does to them.
struct TreeState {
    TreeItem items[kMaxTreeItems] = {};
    int nItems = 0;
    // `entries`: item indices, in the order they are shown. A collapsed
    // folder's descendants are not in it at all, which is what makes the row
    // count the tree's own.
    int entries[kMaxTreeItems] = {};
    int nEntries = 0;
    int selected = -1;
    int rightClicked = -1;
    // uniform_list: every row is the same height, so the offset of a row is
    // an index times this.
    float rowH = 32;
    float scrollY = 0;
    // The last height the list was laid out at, which is what scroll_to_item
    // measures against.
    float viewportH = 0;
    Listener onEvent;
    // cx.emit needs to know who is emitting, and Rust's Context<Self> does.
    // The element stamps this as it builds, so a state can send an event to
    // its subscribers without the caller carrying its handle around.
    EntityId self = {};

    static void OnRowClick(TreeState* self, Ctx* cx, const ClickEvent* ev,
                           intptr_t entryIx);
    static void OnRowMouseDown(TreeState* self, Ctx* cx,
                               const MouseDownEvent* ev, intptr_t entryIx);
    static void OnScroll(TreeState* self, Ctx* cx, const ScrollEvent* ev);
};

// Build the tree. `parent` is an item index, or -1 for a root; the children of
// an item must be added after it. Answers the new item's index.
int TreeAddItem(TreeState* s, Str id, Str label, int parent);
// replace_items + add_entry: walk the roots depth-first and take in the
// children of every expanded folder. Called for you by everything that
// changes an expansion; call it yourself after building the tree.
void TreeRebuild(TreeState* s);
// Where the item with this id is in the flattened list, or -1 — Rust's
// `index_of`, which also only sees what is on screen.
int TreeIndexOf(const TreeState* s, Str id);
// The item behind a row, or null.
const TreeItem* TreeEntryItem(const TreeState* s, int entryIx);

// toggle_expand, without a window to notify. Answers false for a leaf or a
// row that is not there; `expandedOut` says which way it went.
bool TreeToggleExpandAt(TreeState* s, int entryIx, bool* expandedOut);
void TreeToggleExpand(TreeState* s, Ctx* cx, int entryIx);
// expand_ancestors: open everything above the item so it has a row, and
// answer where that row is.
int TreeRevealItem(TreeState* s, Str id);
// scroll_to_item, against the last height the list was laid out at.
void TreeScrollToItem(TreeState* s, int entryIx, ScrollStrategy strategy);
void TreeSetSelected(TreeState* s, Ctx* cx, int entryIx);
// on_entry_click: select the row, then toggle it — so a press on a folder
// opens it and a press on a leaf only selects.
void TreeClickEntry(TreeState* s, Ctx* cx, int entryIx);
void TreePerform(TreeState* s, Ctx* cx, TreeAction act);

void TreeOnAction(TreeState* self, Ctx* cx, const ActionEvent* ev);
void TreeBindKeys(Ctx* cx, El* root, Entity<TreeState> state);

struct Tree {
    static El* New(Ctx* cx);
};
// on_entry_click selects the entry and toggles it, so a press on a folder's
// row opens it and a press on a leaf just selects.
struct TreeItemEl {
    static El* New(Ctx* cx, Str id = {}, Listener onClick = {});
};
} // namespace gpui
