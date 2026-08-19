#include "ui/searchable_list.h"

namespace gpui {

namespace component {

SearchableList* SearchableList::New(Ctx* cx, Entity<ListState> state,
                                    InputState* query) {
    Arena* a = cx->a;
    SearchableList* s = ArenaNew<SearchableList>(a);
    s->a = a;
    s->cx = cx;
    s->state = state;
    s->query = query;
    return s;
}
SearchableList* SearchableList::Item(Str s) {
    if (n < 32) {
        items[n++] = s;
    }
    return this;
}
SearchableList* SearchableList::OnQueryFocus(Listener fn) {
    onQueryFocus = fn;
    return this;
}

// The delegate: the rows that survived the query, one at a time. The matches
// are worked out once and kept on the list, since `render_item` is asked for
// them by position.
static ListItem* RenderMatch(Ctx* cx, void* data, int, int, int entry) {
    const Theme& th = cx->theme();
    SearchableList* self = (SearchableList*)data;
    if (entry < 0 || entry >= self->nMatches) {
        return nullptr;
    }
    return ListItem::New(cx, TextEl(cx->a, self->items[self->matches[entry]])
                                 ->Font(14)
                                 ->Fg(th.foreground));
}

El* SearchableList::IntoEl() {
    List* list = List::New(cx, StrL("searchable-list"), state)
                     ->Searchable(query, onQueryFocus);
    // perform_search: the substring match this façade needs and nothing more.
    const char* q = query ? InputCStr(query) : "";
    nMatches = 0;
    for (int i = 0; i < n; i++) {
        if (q[0] && items[i].s && !strstr(items[i].s, q)) {
            continue;
        }
        matches[nMatches++] = i;
    }
    list->Count(nMatches)->Items(this, &RenderMatch);
    return list->IntoEl();
}

} // namespace component
} // namespace gpui
