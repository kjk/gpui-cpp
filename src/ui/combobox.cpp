#include "ui/i18n.h"
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
Combobox* Combobox::CheckIcon(IconName n) {
    checkIcon = n;
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

Combobox* Combobox::Trigger(El* e) {
    trigger = e;
    return this;
}
Combobox* Combobox::Footer(El* e) {
    footer = e;
    return this;
}
Combobox* Combobox::MaxSelected(int n) {
    if (SearchableListState* s = state.Get(cx)) {
        s->maxSelected = n;
    }
    return this;
}

El* Combobox::IntoEl() {
    // A ComboBox is a Select that is always searchable, so it is one: the
    // trigger, the list and the popup are all the Select's, and the query
    // field is what tells them apart.
    // t!("ComboBox.search_placeholder") where the caller named none, which
    // is where Rust's own default comes from too.
    if (query) {
        InputSetPlaceholder(query, searchPlaceholder.s
                                       ? searchPlaceholder
                                       : Tr("ComboBox.search_placeholder"));
    }
    Select* sel = Select::New(cx, id, state)
                      ->Items(items, nItems)
                      ->Placeholder(placeholder)
                      ->Empty(empty.s ? empty : Tr("ComboBox.empty"))
                      ->W(width)
                      ->Icon(icon)
                      ->CheckIcon(checkIcon)
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
    if (trigger) {
        sel->Trigger(trigger);
    }
    if (footer) {
        sel->Footer(footer);
    }
    return sel->IntoEl();
}

} // namespace component
} // namespace gpui
