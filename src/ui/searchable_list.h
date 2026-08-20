/* Themed searchable list — crates/ui/src/searchable_list

   The machinery behind a Select and a ComboBox: a list with a query field,
   items in sections, one or many of them selected, and the hooks that decide
   what a click on a row changes. Rust makes it a delegate over a ListState;
   the items are the caller's array here, and the state is what is searched,
   what is selected and whether the list is open. */

#include "ui/list.h"

namespace gpui {

namespace component {

const int kMaxSearchableItems = 64;
const int kMaxSearchableSelection = 16;

// One row. `value` is what identifies it — Rust's `SearchableListItem::value`,
// which is what a selection is compared by, and `title` is what it shows.
struct SearchableItem {
    Str title = {};
    Str value = {};
    // The section it belongs to. Items are given in section order.
    int section = 0;
    bool disabled = false;
};

// Single replaces the selection, Multi toggles the row that was clicked.
enum class SearchableListMode : uint8_t {
    Single,
    Multi
};

// SearchableListChange: the atomic changes the mode works a click out to,
// which the delegate is then free to apply, ignore or rewrite.
enum class SearchableListChangeKind : uint8_t {
    Select,
    Deselect
};

struct SearchableListChange {
    SearchableListChangeKind kind = SearchableListChangeKind::Select;
    int index = 0;
};

// SearchableListItem::matches: a case-insensitive substring of the title. An
// empty query matches everything, as `contains("")` does.
bool SearchableItemMatches(const SearchableItem* it, Str query);

struct SearchableListState {
    // The row highlight and the arrow keys are a list's, so the list is what
    // holds them.
    ListState list = {};
    SearchableListMode mode = SearchableListMode::Single;
    // Which items are selected, as indices into the caller's array.
    int selected[kMaxSearchableSelection] = {};
    int nSelected = 0;
    bool open = false;
    // close_on_select: a single-select list closes when something is picked.
    bool closeOnSelect = true;
    // Which items the query left, and how many — the matches the rows are
    // built from, worked out once per frame.
    int matches[kMaxSearchableItems] = {};
    int nMatches = 0;
    // The items being shown, written by whatever renders the list each frame
    // so a click can work out what it changed. They have to outlive the frame:
    // a static array, not one built on the frame arena.
    const SearchableItem* items = nullptr;
    int nItems = 0;
    // What the caller hears once a click has been applied, carrying the item
    // it was about. Rust's `on_confirm`.
    Listener onChange = {};

    static void OnRowClick(SearchableListState* self, Ctx* cx,
                           const ClickEvent* ev, intptr_t match);
};

// The changes a click on `index` comes to under this mode. Single deselects
// whatever was selected and selects the one clicked; Multi toggles it — by
// the item's value, so a second row carrying a selected value toggles that
// value off rather than adding it again. Answers how many were written.
int SearchableListChangesFor(const SearchableListState* s,
                             const SearchableItem* items, int nItems, int index,
                             SearchableListChange* out, int cap);
// on_will_change's default: apply them in order. A Select whose value is
// already in the selection changes nothing, and a Deselect takes out whatever
// carries that value — by value first, then by index, which is what Rust's
// two branches do.
void SearchableListApply(SearchableListState* s, const SearchableItem* items,
                         int nItems, const SearchableListChange* changes,
                         int n);
// Whether the item at `index` carries a value that is selected.
bool SearchableListIsChecked(const SearchableListState* s,
                             const SearchableItem* items, int nItems,
                             int index);
// A click on the item at `index`: the changes its mode comes to, applied.
// Answers whether the list should close, which is `close_on_select` for a
// single-select list and never for a multiple one.
bool SearchableListClick(SearchableListState* s, int index);
// perform_search: which items the query leaves, written into `matches`.
void SearchableListSearch(SearchableListState* s, const SearchableItem* items,
                          int nItems, Str query);

struct SearchableList {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<SearchableListState> state = {};
    const SearchableItem* items = nullptr;
    int nItems = 0;
    // The section headings, one per section index; null for a list with none.
    const Str* sections = nullptr;
    int nSections = 0;
    InputState* query = nullptr;
    Listener onQueryFocus = {};
    El* empty = nullptr;
    float w = 240;
    float maxH = 320;

    static SearchableList* New(Ctx* cx, Str id, Entity<SearchableListState> st,
                               InputState* query);
    SearchableList* Items(const SearchableItem* items, int n);
    SearchableList* Sections(const Str* titles, int n);
    SearchableList* OnQueryFocus(Listener fn);
    SearchableList* Empty(El* e);
    SearchableList* W(float v);
    SearchableList* MaxH(float v);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
