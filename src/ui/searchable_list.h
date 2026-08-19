/* Themed searchable list — crates/ui/src/searchable_list */

#include "ui/list.h"

namespace gpui {

namespace component {

// A list with its own query field. Rust's SearchableListState wraps a
// ListState with the delegate that does the filtering; the filtering here is
// the substring match this façade needs and nothing more.
struct SearchableList {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Entity<ListState> state = {};
    InputState* query = nullptr;
    Listener onQueryFocus;
    Str items[32] = {};
    int n = 0;
    // Which of them the query left, which is what the list asks for by
    // position.
    int matches[32] = {};
    int nMatches = 0;

    static SearchableList* New(Ctx* cx, Entity<ListState> state,
                               InputState* query);
    SearchableList* Item(Str s);
    SearchableList* OnQueryFocus(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
