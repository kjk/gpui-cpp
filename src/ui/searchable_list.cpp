#include "ui/searchable_list.h"
#include "ui/input.h"

namespace gpui {

namespace component {

bool SearchableItemMatches(const SearchableItem* it, Str query) {
    if (query.len <= 0) {
        return true;
    }
    return StrContainsI(it->title, query);
}

void SearchableListSearch(SearchableListState* s, const SearchableItem* items,
                          int nItems, Str query) {
    // The list keeps what it was given, so a click later on knows what it is
    // changing.
    s->items = items;
    s->nItems = nItems;
    s->nMatches = 0;
    for (int i = 0; i < nItems && i < kMaxSearchableItems; i++) {
        if (SearchableItemMatches(&items[i], query)) {
            s->matches[s->nMatches++] = i;
        }
    }
    s->list.count = s->nMatches;
}

static int SelectionIndexOfValue(const SearchableListState* s,
                                 const SearchableItem* items, int nItems,
                                 Str value) {
    for (int i = 0; i < s->nSelected; i++) {
        int ix = s->selected[i];
        if (ix >= 0 && ix < nItems && StrSame(items[ix].value, value)) {
            return i;
        }
    }
    return -1;
}

bool SearchableListIsChecked(const SearchableListState* s,
                             const SearchableItem* items, int nItems,
                             int index) {
    if (index < 0 || index >= nItems) {
        return false;
    }
    return SelectionIndexOfValue(s, items, nItems, items[index].value) >= 0;
}

int SearchableListChangesFor(const SearchableListState* s,
                             const SearchableItem* items, int nItems, int index,
                             SearchableListChange* out, int cap) {
    int n = 0;
    if (s->mode == SearchableListMode::Single) {
        // The single-select strategy: everything that was selected comes out,
        // and the one that was clicked goes in.
        for (int i = 0; i < s->nSelected && n < cap; i++) {
            out[n++] = {SearchableListChangeKind::Deselect, s->selected[i]};
        }
        if (n < cap) {
            out[n++] = {SearchableListChangeKind::Select, index};
        }
        return n;
    }
    // Multi toggles the row that was clicked and leaves the rest alone. What
    // it is toggling is the item's value, which is what the check beside it
    // goes by.
    bool selected = SearchableListIsChecked(s, items, nItems, index);
    if (n < cap) {
        out[n++] = {selected ? SearchableListChangeKind::Deselect
                             : SearchableListChangeKind::Select,
                    index};
    }
    return n;
}

static void SelectionRemoveAt(SearchableListState* s, int at) {
    for (int i = at; i < s->nSelected - 1; i++) {
        s->selected[i] = s->selected[i + 1];
    }
    s->nSelected--;
}

void SearchableListApply(SearchableListState* s, const SearchableItem* items,
                         int nItems, const SearchableListChange* changes,
                         int n) {
    for (int c = 0; c < n; c++) {
        const SearchableListChange& ch = changes[c];
        if (ch.index < 0 || ch.index >= nItems) {
            continue;
        }
        Str value = items[ch.index].value;
        int at = SelectionIndexOfValue(s, items, nItems, value);
        if (ch.kind == SearchableListChangeKind::Select) {
            // A value already in the selection is not added twice.
            if (at < 0 && s->nSelected < kMaxSearchableSelection) {
                s->selected[s->nSelected++] = ch.index;
            }
            continue;
        }
        if (at >= 0) {
            SelectionRemoveAt(s, at);
            continue;
        }
        // Nothing carried that value, so the index itself is what goes.
        for (int i = 0; i < s->nSelected; i++) {
            if (s->selected[i] == ch.index) {
                SelectionRemoveAt(s, i);
                break;
            }
        }
    }
}

bool SearchableListClick(SearchableListState* s, int index) {
    SearchableListChange changes[kMaxSearchableSelection + 1];
    int n = SearchableListChangesFor(s, s->items, s->nItems, index, changes,
                                     (int)(sizeof(changes) / sizeof(*changes)));
    SearchableListApply(s, s->items, s->nItems, changes, n);
    return s->mode == SearchableListMode::Single && s->closeOnSelect;
}

void SearchableListState::OnRowClick(SearchableListState* self, Ctx* cx,
                                     const ClickEvent*, intptr_t match) {
    int m = (int)match;
    if (m < 0 || m >= self->nMatches) {
        return;
    }
    self->list.selected = m;
    int index = self->matches[m];
    // The changes the mode came to are applied here, since the list is what
    // holds both the selection and the items. What the caller hears is what
    // was picked, once it has been.
    if (SearchableListClick(self, index)) {
        self->open = false;
    }
    ListEvent ev = {ListEventKind::Confirm, index, false};
    if (self->onChange.IsValid()) {
        ListenerCall(cx->app, cx->win, self->onChange, &ev);
    }
    Notify(cx);
}

SearchableList* SearchableList::New(Ctx* cx, Str id,
                                    Entity<SearchableListState> st,
                                    InputState* query) {
    Arena* a = cx->a;
    SearchableList* s = ArenaNew<SearchableList>(a);
    s->a = a;
    s->cx = cx;
    s->id = id;
    s->state = st;
    s->query = query;
    return s;
}
SearchableList* SearchableList::Items(const SearchableItem* it, int n) {
    items = it;
    nItems = n;
    return this;
}
SearchableList* SearchableList::Sections(const Str* titles, int n) {
    sections = titles;
    nSections = n;
    return this;
}
SearchableList* SearchableList::OnQueryFocus(Listener fn) {
    onQueryFocus = fn;
    return this;
}
SearchableList* SearchableList::Empty(El* e) {
    empty = e;
    return this;
}
SearchableList* SearchableList::W(float v) {
    w = v;
    return this;
}
SearchableList* SearchableList::MaxH(float v) {
    maxH = v;
    return this;
}

El* SearchableList::IntoEl() {
    const Theme& th = cx->theme();
    SearchableListState* s = state.Get(cx);
    El* box = Div(a)
                  ->FlexCol()
                  ->W(w)
                  ->Pad(4)
                  ->Gap(2)
                  ->Radius(th.radius)
                  ->Border(1, th.border)
                  ->Bg(th.background);
    if (!s) {
        return box;
    }
    // perform_search: the query decides which items there are to show at all.
    SearchableListSearch(s, items, nItems, query ? InputValue(query) : Str{});

    if (query) {
        El* row =
            Div(a)->FlexRow()->W(kFill)->H(32)->PadX(4)->Gap(8)->ItemsCenter();
        row->Child(IconEl(a, IconName::Search, 16)->Fg(th.mutedFg));
        row->Child(Div(a)->Grow()->Child(
            Input::New(cx, StrDup(a, fmt("%s-query", id)), query)
                ->Appearance(false)
                ->OnFocus(onQueryFocus)
                ->IntoEl()));
        row->BorderB(1, th.border);
        box->Child(row);
    }

    if (s->nMatches == 0) {
        box->Child(empty
                       ? empty
                       : Div(a)
                             ->FlexCol()
                             ->W(kFill)
                             ->PadY(24)
                             ->ItemsCenter()
                             ->JustifyCenter()
                             ->Child(IconEl(a, IconName::Inbox, 32)
                                         ->Fg(RgbaOpacity(th.mutedFg, 0.6f))));
        return box;
    }

    El* rows = Div(a)->FlexCol()->W(kFill)->MaxH(maxH)->ClipY();
    Listener click = ListenTo(state, &SearchableListState::OnRowClick, 0);
    int lastSection = -1;
    for (int m = 0; m < s->nMatches; m++) {
        int ix = s->matches[m];
        const SearchableItem& it = items[ix];
        // render_section_header: a heading whenever the section changes, and
        // only for the sections the query left something in.
        if (sections && it.section != lastSection && it.section < nSections) {
            rows->Child(Div(a)->W(kFill)->PadX(8)->PadT(8)->PadB(4)->Child(
                TextEl(a, sections[it.section])->Font(12)->Fg(th.mutedFg)));
            lastSection = it.section;
        }
        bool checked = SearchableListIsChecked(s, items, nItems, ix);
        El* row = Div(a)
                      ->FlexRow()
                      ->W(kFill)
                      ->H(30)
                      ->PadX(8)
                      ->Gap(8)
                      ->ItemsCenter()
                      ->JustifyBetween()
                      ->Radius(th.radius);
        if (!it.disabled) {
            row->HoverBg(th.accent);
        }
        if (m == s->list.selected) {
            row->Bg(th.accent);
        }
        row->Child(TextEl(a, it.title)
                       ->Font(14)
                       ->Fg(it.disabled ? th.mutedFg : th.foreground));
        // The trailing check the adapter adds for a selected row.
        if (checked) {
            row->Child(IconEl(a, IconName::Check, 16)->Fg(th.foreground));
        }
        if (!it.disabled) {
            BindClick(row, StrDup(a, fmt("%s-row-%d", id, ix)),
                      ListenerArg(click, m));
        }
        rows->Child(row);
    }
    box->Child(rows);
    return box;
}

} // namespace component
} // namespace gpui
