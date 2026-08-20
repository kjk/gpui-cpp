/* Themed searchable list — crates/ui/src/searchable_list

   The machinery behind a Select and a ComboBox: a list with a query field,
   items in sections, one or many of them selected, and the hooks that decide
   what a click on a row changes. Rust makes it a delegate over a ListState;
   the items are the caller's array here, and the state is what is searched,
   what is selected and whether the list is open. */

#include "ui/list.h"

namespace gpui {

namespace component {

// One row. `value` is what identifies it — Rust's `SearchableListItem::value`,
// which is what a selection is compared by, and `title` is what it shows.
struct SearchableItem {
    Str title = {};
    Str value = {};
    // The section it belongs to. Items are given in section order.
    int section = 0;
    bool disabled = false;
    // SearchableListItem::render: the story's Industry rows draw a small
    // muted icon before the label, which is all any of them add.
    IconName icon = IconName::None;
    // is_item_checked / is_item_enabled: a row whose check is somebody else's
    // to decide. It always reads as selected and never answers a click, which
    // is what a delegate that pins an item does with the two hooks.
    bool pinned = false;
    // render_item: the pill a row draws after its label. Rust's delegate
    // returns a whole element for the row; the only thing any of them add is
    // this badge, so the item carries the text instead.
    Str badge = {};
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
    ListState list;
    SearchableListMode mode = SearchableListMode::Single;
    // Which items are selected, as indices into the caller's array. It grows
    // with the selection: a list is as long as the caller's array, and a
    // multi-select one can have all of it picked.
    Vec<int> selected;
    bool open = false;
    // close_on_select: a single-select list closes when something is picked.
    bool closeOnSelect = true;
    // Which items the query left — the matches the rows are built from,
    // worked out once per frame.
    Vec<int> matches;
    // The items being shown, written by whatever renders the list each frame
    // so a click can work out what it changed. They have to outlive the frame:
    // a static array, not one built on the frame arena.
    const SearchableItem* items = nullptr;
    int nItems = 0;
    // on_will_change: how many values the selection will hold. A Select past
    // the limit is dropped rather than applied, and the rows that are not
    // already in it stop answering. 0 is no limit.
    int maxSelected = 0;
    // What the caller hears once a click has been applied, carrying the item
    // it was about. Rust's `on_confirm`.
    Listener onChange = {};
    // The select's focus handles. Rust's Select focuses its `content` handle
    // when the list comes up — `tracked_focus_handle` — and puts focus back
    // on the trigger when it goes away; these are the ids that stand in for
    // the two handles, filled in by whatever renders the select, plus where
    // focus was before it opened.
    int triggerFocusId = 0;
    int contentFocusId = 0;
    int previousFocusId = 0;

    static void OnRowClick(SearchableListState* self, Ctx* cx,
                           const ClickEvent* ev, intptr_t match);

    // The Select's five bindings, arriving as actions. Rust's Select root
    // hears Confirm and Cancel and hands the arrows to the content it
    // focused; this is that root and that content in one, since the list and
    // the select share a state here.
    static void OnAction(SearchableListState* self, Ctx* cx,
                         const ActionEvent* ev);
    // The same, for a list that is not inside a select: the five "List"
    // bindings, over the row highlight this state already holds.
    static void OnListAction(SearchableListState* self, Ctx* cx,
                             const ActionEvent* ev);

    ~SearchableListState() {
        selected.Reset();
        matches.Reset();
    }
};

// The selection replaced by the one index, which is what a single-select list
// holds and what a caller seeding one wants. A negative index selects nothing.
void SearchableListSelectOnly(SearchableListState* s, int index);

// The changes a click on `index` comes to under this mode. Single deselects
// whatever was selected and selects the one clicked; Multi toggles it — by
// the item's value, so a second row carrying a selected value toggles that
// value off rather than adding it again. `out` is cleared first.
void SearchableListChangesFor(const SearchableListState* s,
                              const SearchableItem* items, int nItems,
                              int index, Vec<SearchableListChange>* out);
// on_will_change's default: apply them in order. A Select whose value is
// already in the selection changes nothing, and a Deselect takes out whatever
// carries that value — by value first, then by index, which is what Rust's
// two branches do.
void SearchableListApply(SearchableListState* s, const SearchableItem* items,
                         int nItems, const SearchableListChange* changes,
                         int n);
// Whether the item at `index` carries a value that is selected. A pinned
// item always does — that is what is_item_checked returning true for it means.
bool SearchableListIsChecked(const SearchableListState* s,
                             const SearchableItem* items, int nItems,
                             int index);
// is_item_enabled: whether a click on the row does anything. A disabled item
// never answers, a pinned one never answers, and once the selection is at its
// limit only the rows already in it do.
bool SearchableListIsEnabled(const SearchableListState* s,
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
    // Combobox::footer: an action under the option list.
    El* footer = nullptr;
    float w = 240;
    float maxH = 320;
    // Combobox::check_icon: what marks a selected row.
    IconName checkIcon = IconName::Check;
    // Whether a Select encloses this list. One inside a select leaves the
    // keyboard to the select's own context — escape there closes the popup
    // rather than clearing the highlight — while one standing on its own
    // declares the "List" context and answers for itself.
    bool inSelect = false;

    static SearchableList* New(Ctx* cx, Str id, Entity<SearchableListState> st,
                               InputState* query);
    SearchableList* Items(const SearchableItem* items, int n);
    SearchableList* Sections(const Str* titles, int n);
    SearchableList* OnQueryFocus(Listener fn);
    SearchableList* Empty(El* e);
    SearchableList* Footer(El* e);
    SearchableList* W(float v);
    SearchableList* MaxH(float v);
    SearchableList* CheckIcon(IconName n);
    SearchableList* InSelect(bool v);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
