/* Unstyled list — crates/ui/src/list/list.rs */

#include "gpui/gpui.h"

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
// answer rather than routed as an action, since there is no action system
// here.
enum class ListAction : uint8_t {
    None,
    SelectPrev,
    SelectNext,
    Confirm,
    Cancel
};

ListAction ListActionForKey(int key);

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
    Listener onEvent = {};

    // Rust's ListState is an Entity, which is what lets the item closures
    // capture it; here the row elements name these handlers instead, so a
    // page holds an Entity<ListState> and the list binds to it.
    static void OnRowClick(ListState* self, Ctx* cx, const ClickEvent* ev,
                           intptr_t ix);
    static void OnRowMouseDown(ListState* self, Ctx* cx,
                               const MouseDownEvent* ev, intptr_t ix);
};

// rows_cache.next / .prev. Both wrap: past the last row is the first, and
// before the first is the last. With nothing selected, next takes the first
// row and prev the last.
int ListNextIndex(const ListState* s);
int ListPrevIndex(const ListState* s);

// The action, applied to the state and reported through `onEvent`.
void ListPerform(ListState* s, Ctx* cx, ListAction act, bool secondary);

// A click on a row. Rust's on_click clears the right-clicked row, selects
// this one and confirms it in one go — a click is a Select and a Confirm.
void ListClickRow(ListState* s, Ctx* cx, int ix, bool secondary);

// A secondary press on a row, which only marks it.
void ListRightClickRow(ListState* s, Ctx* cx, int ix);

} // namespace gpui
