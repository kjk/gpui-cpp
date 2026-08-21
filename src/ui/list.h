/* Themed list — crates/ui/src/list */

#include "ui/sizing.h"
#include "base/list.h"

namespace gpui {

namespace component {

// One row. Rust's ListItem is what a delegate's render_item hands back: the
// content is the caller's and the selected / confirmed styling is the list's.
// list/loading.rs: the skeleton view a loading list shows, which is what
// `render_loading` returns when a delegate does not replace it.
El* ListLoadingView(Ctx* cx, float h = 0);

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

// list/separator_item.rs: a row that is a rule rather than an item. It is a
// disabled ListItem in Rust, so it is never selected and never answers a
// click; the caller's children are what it draws.
ListItem* ListSeparatorItem(Ctx* cx, El* child = nullptr);

// The list, bound to the ListState that answers its keys and clicks the way
// an Input is bound to an InputState. The rows come from the caller one at a
// time rather than all at once: only the ones the viewport can show are ever
// built, which is what `ListDelegate::render_item` over a virtual list is.
struct List {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<ListState> state = {};
    // The delegate. Rust's is a trait with closures over the view; an element
    // here carries no closure, so it is a pointer to the caller's data and
    // the three render functions that read it.
    void* data = nullptr;
    ListItem* (*item)(Ctx* cx, void* data, int section, int row,
                      int entry) = nullptr;
    El* (*header)(Ctx* cx, void* data, int section) = nullptr;
    El* (*footer)(Ctx* cx, void* data, int section) = nullptr;
    // The search field, when the list is searchable.
    InputState* search = nullptr;
    Listener onSearchFocus;
    // render_loading / render_initial: what the list shows instead of its
    // rows while the delegate is loading, and before anything has been
    // searched for. Null takes the built-in skeleton view for the first and
    // nothing at all for the second, which are Rust's defaults.
    El* loading = nullptr;
    El* initial = nullptr;
    // render_empty: what to show when there is nothing in the list. Null
    // takes Rust's own — a muted Inbox icon.
    El* empty = nullptr;
    float h = 320;

    static List* New(Ctx* cx, Str id, Entity<ListState> state);
    // The sections and their item counts, which the state flattens.
    List* Sections(const int* counts, int n);
    List* Count(int n);
    List* Items(void* data,
                ListItem* (*fn)(Ctx*, void*, int section, int row, int entry));
    List* Headers(El* (*headerFn)(Ctx*, void*, int),
                  El* (*footerFn)(Ctx*, void*, int) = nullptr);
    List* Searchable(InputState* search, Listener onFocus);
    List* Loading(El* e);
    List* Initial(El* e);
    List* Empty(El* e);
    List* H(float px);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
