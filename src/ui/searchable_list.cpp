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

El* SearchableList::IntoEl() {
    const Theme& th = cx->theme();
    List* list = List::New(cx, StrL("searchable-list"), state)
                     ->Searchable(query, onQueryFocus);
    const char* q = query ? InputCStr(query) : "";
    for (int i = 0; i < n; i++) {
        if (q[0] && items[i].s && !strstr(items[i].s, q)) {
            continue;
        }
        list->Item(ListItem::New(
            cx, TextEl(a, items[i])->Font(14)->Fg(th.foreground)));
    }
    return list->IntoEl();
}

} // namespace component
} // namespace gpui
