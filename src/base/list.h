/* Unstyled list — crates/ui/src/list/list.rs */

#include "gpui/gpui.h"
#include "base/index_path.h"
#include "base/virtual_list.h"

namespace gpui {

// ListEvent, what the list tells whoever is listening. Rust emits these
// through `cx.emit` and a caller subscribes; here the state carries one
// listener, the way InputState and SliderState do.
enum class ListEventKind : uint8_t {
    // The selection moved, by a key or by a click.
    Select,
    // A row was taken: Enter on the selected row, or a click on any row.
    Confirm,
    // Escape.
    Cancel
};

struct ListEvent {
    ListEventKind kind = ListEventKind::Select;
    // The row it is about; -1 for a Cancel that cleared the selection.
    int index = -1;
    // Confirm { secondary }: the modifier that means "the other way" —
    // Command on macOS, Control elsewhere.
    bool secondary = false;
};

// What a keystroke asks a list to do. Rust binds up, down, enter, escape and
// secondary-enter in the "List" key context; this is that table, read as an
// answer rather than routed as an action.
enum class ListAction : uint8_t {
    None,
    SelectPrev,
    SelectNext,
    Confirm,
    Cancel
};

// Rust's Confirm carries a field — `Confirm { secondary }`, bound twice, once
// to enter and once to secondary-enter — so the answer here is the pair. The
// modifier is the platform's shortcut key, which is `ctrl` on a KeyEvent.
struct ListKeyAction {
    ListAction action = ListAction::None;
    bool secondary = false;
};

// list.rs::init: escape, enter, secondary-enter, up and down in the "List"
// key context.
void ListInitKeys();
Str ListContext();

// The action, read as what the list does about it. Rust's Confirm carries a
// field — `Confirm { secondary }`, bound twice — so the two bindings are two
// names here and the flag comes back beside the answer.
ListKeyAction ListActionOf(uint32_t id);

// RowEntry, from crates/ui/src/list/cache.rs: what one row of the flattened
// list is. A section contributes a header, its items and a footer, and a
// section with no items contributes nothing at all.
enum class ListRowKind : uint8_t {
    Entry,
    SectionHeader,
    SectionFooter
};

struct ListRow {
    ListRowKind kind = ListRowKind::Entry;
    int section = 0;
    // Which row of its section this is, and which item of the whole list —
    // Rust's IndexPath and the position the selection is kept as.
    int row = 0;
    int entry = -1;

    // The same place under the name Rust addresses it by. A header or a
    // footer has a section but no row, which is what its `row` of 0 means.
    IndexPath Path() const { return IndexPath{section, row, 0}; }
};

// What a list is between frames. Rust splits this across ListState and the
// delegate; the rows themselves stay with the caller either way, so this is
// the part that answers keys and clicks.
struct ListState {
    // How many rows there are, which the caller sets every frame.
    int count = 0;
    // selected_index: none is -1.
    int selected = -1;
    // right_clicked_index: the row under a secondary press, which paints as
    // secondary_selected until the next click.
    int rightClicked = -1;
    bool selectable = true;
    // reset_on_cancel: whether Escape clears the selection or only reports.
    bool resetOnCancel = true;
    // The sections, as the number of items in each. RowsCache keeps the same
    // list and flattens it; a section with no items is skipped, header and
    // footer with it.
    Vec<int> sectionCounts;
    bool sectionHeaders = false;
    bool sectionFooters = false;
    // The list is virtualized, and uniform_list wants every row the same
    // height. `viewportH` is what the last frame was laid out at, which is
    // what scroll_to_item measures against.
    float rowH = 32;
    float scrollY = 0;
    float viewportH = 0;
    // delegate.loading() / has_more() / load_more_threshold(): the rows are
    // replaced by a loading view, and coming within `loadMoreThreshold` rows
    // of the end asks for more.
    bool loading = false;
    bool hasMore = false;
    int loadMoreThreshold = 20;
    Listener onEvent = {};
    Listener onLoadMore = {};
    // cx.emit needs to know who is emitting, and Rust's Context<Self> does.
    // The element stamps this as it builds, so a state can send an event to
    // its subscribers without the caller carrying its handle around.
    EntityId self = {};

    ~ListState() { sectionCounts.Reset(); }

    // Rust's ListState is an Entity, which is what lets the item closures
    // capture it; here the row elements name these handlers instead, so a
    // page holds an Entity<ListState> and the list binds to it.
    static void OnRowClick(ListState* self, Ctx* cx, const ClickEvent* ev,
                           intptr_t ix);
    static void OnRowMouseDown(ListState* self, Ctx* cx,
                               const MouseDownEvent* ev, intptr_t ix);
    static void OnScroll(ListState* self, Ctx* cx, const ScrollEvent* ev);
};

// The sections and their item counts, which is what RowsCache is built from.
// `count` follows: the items of every section added up.
void ListSetSections(ListState* s, const int* counts, int n, bool headers,
                     bool footers);
// One section holding every item, which is what a list with no sections of
// its own is.
void ListSetCount(ListState* s, int count);
// The flattened rows: every section's header, its items and its footer.
int ListRowCount(const ListState* s);
// What the row at this position is. Rust keeps the answers in a vector; there
// are at most sixteen sections to walk here, so they are worked out instead.
ListRow ListRowAt(const ListState* s, int rowIx);
// Where an item sits among the flattened rows, or -1.
int ListRowOfEntry(const ListState* s, int entry);
// The two directions between the flat entry index this tree keys on and the
// IndexPath Rust keys on. An entry outside the list is section 0, row -1;
// a path outside it is -1.
IndexPath ListIndexPathOf(const ListState* s, int entry);
int ListEntryOf(const ListState* s, IndexPath path);
// scroll_to_item, against the height the list was last laid out at.
void ListScrollToItem(ListState* s, int entry, ScrollStrategy strategy);
// load_more: true when the last row built is within the threshold of the end
// and there is more to come. Rust asks this while it renders the visible
// range, and so does the list here.
bool ListShouldLoadMore(const ListState* s, int lastVisibleRow);

// rows_cache.next / .prev. Both wrap: past the last row is the first, and
// before the first is the last. With nothing selected, next takes the first
// row and prev the last.
int ListNextIndex(const ListState* s);
int ListPrevIndex(const ListState* s);

// The action, applied to the state and reported through `onEvent`.
void ListPerform(ListState* s, Ctx* cx, ListAction act, bool secondary);

// The five bindings, arriving as actions on the element that declares the
// context. Rust hangs one on_action per action off the list; there is one id
// to switch on here.
void ListOnAction(ListState* self, Ctx* cx, const ActionEvent* ev);

// Declare the "List" key context on `root` and hang the handlers off it.
void ListBindKeys(Ctx* cx, El* root, Entity<ListState> state);

// A click on a row. Rust's on_click clears the right-clicked row, selects
// this one and confirms it in one go — a click is a Select and a Confirm.
void ListClickRow(ListState* s, Ctx* cx, int ix, bool secondary);

// A secondary press on a row, which only marks it.
void ListRightClickRow(ListState* s, Ctx* cx, int ix);

} // namespace gpui
