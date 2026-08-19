/* Themed list — crates/ui/src/list */

#include "ui/sizing.h"
#include "base/list.h"

namespace gpui {

namespace component {

// One row. Rust's ListItem is what a delegate's render_item hands back: the
// content is the caller's and the selected / confirmed styling is the list's.
struct ListItem {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    El* child = nullptr;
    bool selected = false;
    bool secondarySelected = false;
    bool confirmed = false;
    bool disabled = false;

    static ListItem* New(Ctx* cx, El* child);
    ListItem* Selected(bool v);
    ListItem* SecondarySelected(bool v);
    ListItem* Confirmed(bool v);
    ListItem* Disabled(bool v);
    El* IntoEl(Str id, Listener onClick, Listener onMouseDown);
};

// A run of rows under a heading, which is what a delegate's section is.
struct ListSection {
    El* header = nullptr;
    El* footer = nullptr;
    ListItem* rows[64] = {};
    int n = 0;
};

// The list itself, bound to the ListState that answers its keys and clicks
// the way an Input is bound to an InputState.
struct List {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<ListState> state = {};
    ListSection sections[8] = {};
    int nSections = 0;
    // The row index the next Item() gets, which is what the state counts in.
    int nRows = 0;
    // The search field, when the list is searchable.
    InputState* search = nullptr;
    Listener onSearchFocus;
    bool loading = false;
    float maxH = 0;

    static List* New(Ctx* cx, Str id, Entity<ListState> state);
    List* Section(El* header, El* footer = nullptr);
    List* Item(ListItem* item);
    List* Searchable(InputState* search, Listener onFocus);
    List* Loading(bool v);
    List* MaxH(float px);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
