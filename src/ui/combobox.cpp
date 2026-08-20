#include "ui/combobox.h"

namespace gpui {

namespace component {

Combobox* Combobox::New(Ctx* cx, Str id, Entity<SearchableListState> state,
                        InputState* query) {
    Arena* a = cx->a;
    Combobox* c = ArenaNew<Combobox>(a);
    c->a = a;
    c->cx = cx;
    c->id = id;
    c->state = state;
    c->query = query;
    return c;
}
Combobox* Combobox::Items(const SearchableItem* it, int n) {
    items = it;
    nItems = n;
    return this;
}
Combobox* Combobox::Sections(const Str* titles, int n) {
    sections = titles;
    nSections = n;
    return this;
}
Combobox* Combobox::Placeholder(Str s) {
    placeholder = s;
    return this;
}
Combobox* Combobox::SearchPlaceholder(Str s) {
    searchPlaceholder = s;
    return this;
}
Combobox* Combobox::Empty(Str s) {
    empty = s;
    return this;
}
Combobox* Combobox::Icon(IconName n) {
    icon = n;
    return this;
}
Combobox* Combobox::W(float v) {
    width = v;
    return this;
}
Combobox* Combobox::MenuMaxH(float v) {
    menuMaxH = v;
    return this;
}
Combobox* Combobox::Disabled(bool v) {
    disabled = v;
    return this;
}
Combobox* Combobox::Cleanable(bool v) {
    cleanable = v;
    return this;
}
Combobox* Combobox::FocusRing(bool v) {
    focusRing = v;
    return this;
}
Combobox* Combobox::Multiple(bool v) {
    SearchableListState* s = state.Get(cx);
    if (s) {
        s->mode = v ? SearchableListMode::Multi : SearchableListMode::Single;
        s->closeOnSelect = !v;
    }
    return this;
}
Combobox* Combobox::OnQueryFocus(Listener fn) {
    onQueryFocus = fn;
    return this;
}
Combobox* Combobox::OnToggle(Listener fn) {
    onToggle = fn;
    return this;
}
Combobox* Combobox::OnClear(Listener fn) {
    onClear = fn;
    return this;
}

El* Combobox::IntoEl() {
    // A ComboBox is a Select that is always searchable, so it is one: the
    // trigger, the list and the popup are all the Select's, and the query
    // field is what tells them apart.
    if (query && searchPlaceholder.s) {
        InputSetPlaceholder(query, searchPlaceholder);
    }
    Select* sel = Select::New(cx, id, state)
                      ->Items(items, nItems)
                      ->Placeholder(placeholder)
                      ->Empty(empty)
                      ->W(width)
                      ->Icon(icon)
                      ->Disabled(disabled)
                      ->Cleanable(cleanable)
                      ->FocusRing(focusRing)
                      ->Searchable(query, onQueryFocus)
                      ->OnToggle(onToggle)
                      ->OnClear(onClear);
    if (sections) {
        sel->Sections(sections, nSections);
    }
    if (menuMaxH > 0) {
        sel->MenuMaxH(menuMaxH);
    }
    return sel->IntoEl();
}

} // namespace component
} // namespace gpui
