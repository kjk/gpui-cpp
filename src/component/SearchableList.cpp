#include "component/SearchableList.h"
#include "component/Input.h"

namespace gpui {

namespace component {

SearchableList* SearchableList::New(Ctx* cx, InputState* query) {
    Arena* a = cx->a;
    SearchableList* s = ArenaNew<SearchableList>(a);
    s->a = a;
    s->cx = cx;
    s->query = query;
    return s;
}
SearchableList* SearchableList::Item(Str s) {
    if (n < 32) {
        items[n++] = s;
    }
    return this;
}
SearchableList* SearchableList::OnSelect(Listener fn) {
    onSelect = fn;
    return this;
}

El* SearchableList::IntoEl() {
    List* list = List::New(cx);
    const char* q = query ? InputCStr(query) : "";
    for (int i = 0; i < n; i++) {
        if (q[0] && items[i].s && !strstr(items[i].s, q)) {
            continue;
        }
        list->Item(items[i]);
    }
    list->OnSelect(onSelect);
    return Div(a)
        ->FlexCol()
        ->Gap(8)
        ->Child(Input::New(cx, StrL("search"), query)->IntoEl())
        ->Child(list->IntoEl());
}

} // namespace component
} // namespace gpui
