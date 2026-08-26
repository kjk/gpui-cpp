#include "ui/searchable_list.h"
#include "base/actions.h"
#include "base/list.h"
#include "base/select.h"
#include "ui/select.h"
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
    s->matches.Clear();
    for (int i = 0; i < nItems; i++) {
        if (SearchableItemMatches(&items[i], query)) {
            s->matches.Append(i);
        }
    }
    s->list.count = s->matches.len;
}

void SearchableListSelectOnly(SearchableListState* s, int index) {
    if (!s) {
        return;
    }
    s->selected.Clear();
    if (index >= 0) {
        s->selected.Append(index);
    }
}

static int SelectionIndexOfValue(const SearchableListState* s,
                                 const SearchableItem* items, int nItems,
                                 Str value) {
    for (int i = 0; i < s->selected.len; i++) {
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
    if (items[index].pinned) {
        return true;
    }
    return SelectionIndexOfValue(s, items, nItems, items[index].value) >= 0;
}

bool SearchableListIsEnabled(const SearchableListState* s,
                             const SearchableItem* items, int nItems,
                             int index) {
    if (index < 0 || index >= nItems) {
        return false;
    }
    if (items[index].disabled || items[index].pinned) {
        return false;
    }
    if (s->maxSelected > 0 && s->selected.len >= s->maxSelected) {
        return SelectionIndexOfValue(s, items, nItems, items[index].value) >= 0;
    }
    return true;
}

void SearchableListChangesFor(const SearchableListState* s,
                              const SearchableItem* items, int nItems,
                              int index, Vec<SearchableListChange>* out) {
    out->Clear();
    if (s->mode == SearchableListMode::Single) {
        // The single-select strategy: everything that was selected comes out,
        // and the one that was clicked goes in.
        for (int i = 0; i < s->selected.len; i++) {
            out->Append({SearchableListChangeKind::Deselect, s->selected[i]});
        }
        out->Append({SearchableListChangeKind::Select, index});
        return;
    }
    // Multi toggles the row that was clicked and leaves the rest alone. What
    // it is toggling is the item's value, which is what the check beside it
    // goes by.
    bool selected = SearchableListIsChecked(s, items, nItems, index);
    out->Append({selected ? SearchableListChangeKind::Deselect
                          : SearchableListChangeKind::Select,
                 index});
}

static void SelectionRemoveAt(SearchableListState* s, int at) {
    for (int i = at; i < s->selected.len - 1; i++) {
        s->selected[i] = s->selected[i + 1];
    }
    s->selected.len--;
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
            // on_will_change: a Select that would take the selection past its
            // limit is dropped, rather than pushing something else out.
            if (s->maxSelected > 0 && s->selected.len >= s->maxSelected) {
                continue;
            }
            // A value already in the selection is not added twice.
            if (at < 0) {
                s->selected.Append(ch.index);
            }
            continue;
        }
        if (at >= 0) {
            SelectionRemoveAt(s, at);
            continue;
        }
        // Nothing carried that value, so the index itself is what goes.
        for (int i = 0; i < s->selected.len; i++) {
            if (s->selected[i] == ch.index) {
                SelectionRemoveAt(s, i);
                break;
            }
        }
    }
}

bool SearchableListClick(SearchableListState* s, int index) {
    Vec<SearchableListChange> changes;
    SearchableListChangesFor(s, s->items, s->nItems, index, &changes);
    SearchableListApply(s, s->items, s->nItems, changes.els, changes.len);
    changes.Reset();
    return s->mode == SearchableListMode::Single && s->closeOnSelect;
}

void SearchableListState::OnRowClick(SearchableListState* self, Ctx* cx,
                                     const ClickEvent*, intptr_t match) {
    int m = (int)match;
    if (m < 0 || m >= self->matches.len) {
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
SearchableList* SearchableList::Footer(El* e) {
    footer = e;
    return this;
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
SearchableList* SearchableList::InSelect(bool v) {
    inSelect = v;
    return this;
}
SearchableList* SearchableList::CheckIcon(IconName n) {
    checkIcon = n;
    return this;
}

El* SearchableList::IntoEl() {
    const Theme& th = ThemeNow(cx->app);
    SearchableListState* s = state.Get(cx);
    El* box = Div(a)
                  ->Id(id)
                  ->FlexCol()
                  ->W(w)
                  ->Pad(4)
                  ->Gap(2)
                  ->Radius(th.radius)
                  ->Border(1, th.border)
                  ->Bg(th.tokens.background);
    if (!s) {
        return box;
    }
    // perform_search: the query decides which items there are to show at all.
    SearchableListSearch(s, items, nItems, query ? InputValue(query) : Str{});

    if (query) {
        El* row =
            Div(a)->FlexRow()->W(kFill)->H(32)->PadX(4)->Gap(8)->ItemsCenter();
        row->Child(IconEl(a, IconName::Search, 16)->Fg(th.mutedFg));
        row->Child(Div(a)->Flex1()->Child(Input::New(cx, StrL("query"), query)
                                              ->Appearance(false)
                                              ->OnFocus(onQueryFocus)
                                              ->IntoEl()));
        row->BorderB(1, th.border);
        box->Child(row);
    }

    if (s->matches.len == 0) {
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
    for (int m = 0; m < s->matches.len; m++) {
        int ix = s->matches[m];
        const SearchableItem& it = items[ix];
        // render_section_header: a heading whenever the section changes, and
        // only for the sections the query left something in.
        if (sections && it.section != lastSection && it.section < nSections) {
            // render_section_header: py_0p5, px_2, text_sm and muted. The
            // list scrolls rather than squeezing, so neither a heading nor a
            // row is a flex item that can give height back.
            rows->Child(Div(a)->W(kFill)->Shrink0()->PadX(8)->PadY(2)->Child(
                TextEl(a, sections[it.section])->Font(14)->Fg(th.mutedFg)));
            lastSection = it.section;
        }
        bool checked = SearchableListIsChecked(s, items, nItems, ix);
        El* row = Div(a)
                      ->FlexRow()
                      ->W(kFill)
                      ->Shrink0()
                      ->PadY(4)
                      ->PadX(8)
                      ->Gap(8)
                      ->ItemsCenter()
                      ->JustifyBetween()
                      ->Radius(th.radius);
        bool enabled = SearchableListIsEnabled(s, items, nItems, ix);
        if (enabled) {
            row->HoverBg(th.tokens.accent);
        }
        if (m == s->list.selected) {
            row->Bg(th.tokens.accent);
        }
        // SearchableListItem::render: an icon before the label when the item
        // gave one.
        El* label = Div(a)->FlexRow()->Gap(8)->ItemsCenter()->MinW(0);
        if (it.icon != IconName::None) {
            label->Child(IconEl(a, it.icon, UiIconPx(UiSize::Small))
                             ->Fg(th.mutedFg));
        }
        label->Child(TextEl(a, it.title)
                         ->Font(14)
                         ->Fg(enabled || checked ? th.foreground : th.mutedFg));
        row->Child(label);
        El* trail = Div(a)->FlexRow()->Gap(4)->ItemsCenter()->Shrink0();
        // The trailing check the adapter adds for a selected row.
        if (checked) {
            trail->Child(IconEl(a, checkIcon, 16)->Fg(th.foreground));
        }
        // render_item's badge, after the check, as the story's delegate puts
        // its "Featured" pill.
        if (it.badge.s) {
            trail->Child(Div(a)
                             ->PadX(4)
                             ->Radius(th.radius * 0.5f)
                             ->Bg(th.tokens.primary)
                             ->Child(TextEl(a, it.badge)
                                         ->Font(12)
                                         ->Fg(th.primaryFg)
                                         ->LineHeight(1.4f)));
        }
        row->Child(trail);
        if (enabled) {
            BindPathClick(row, StrDup(a, fmt("row-%d", ix)),
                          ListenerArg(click, m));
        }
        rows->Child(row);
    }
    box->Child(rows);
    if (footer) {
        // Combobox::footer: an action under the list, ruled off from it.
        box->Child(
            Div(a)->W(kFill)->BorderT(1, th.border)->Pad(4)->Child(footer));
    }
    // The list's own key context, for one that is not inside a select. The
    // rows are focusable, and so is the box, so a chord finds it whether a
    // row was clicked or the list was tabbed to.
    if (!inSelect) {
        ListInitKeys();
        Listener onAction = ListenTo(state, &SearchableListState::OnListAction);
        box->FocusId(HashClickId(id))
            ->FocusRing(false)
            ->FocusOnPress()
            ->KeyContext(ListContext())
            ->OnAction(action::Cancel(), onAction)
            ->OnAction(action::Confirm(), onAction)
            ->OnAction(action::SelectUp(), onAction)
            ->OnAction(action::SelectDown(), onAction);
    } else if (s && s->contentFocus.IsValid()) {
        // `content_focus_handle` is the *list's* handle upstream —
        // `state.list.focus_handle(cx)` — tracked on the list's own element,
        // so the focus a select moves into its dropdown lands on something
        // the frame can name. It named nothing here: the select set
        // `win->focusId` to a handle no element carried, and only the id
        // comparison in WindowFocusWithin made closing look like it worked.
        // With the handle on the box, focus that has gone on into the query
        // field is still the list's, so closing puts it back where it was
        // rather than leaving it on an input that has gone.
        //
        // Not a tab stop: `track_focus` alone only makes an element
        // focusable, and upstream asks for `.tab_stop(true)` on the trigger
        // and nowhere else — so Tab still walks past an open dropdown rather
        // than into it.
        box->TrackFocus(s->contentFocus)->TabStop(false)->FocusRing(false);
    }
    return box;
}

void SearchableListState::OnAction(SearchableListState* self, Ctx* cx,
                                   const ActionEvent* ev) {
    if (!self) {
        return;
    }
    // Once it is open the root has nothing left to do with an arrow, so the
    // list takes it — which is what Rust's content focus handle is for.
    if (self->open && (ev->action == action::SelectUp() ||
                       ev->action == action::SelectDown())) {
        ListPerform(&self->list, cx,
                    ev->action == action::SelectDown() ? ListAction::SelectNext
                                                       : ListAction::SelectPrev,
                    false);
        return;
    }
    switch (SelectActionOf(ev->action, self->open, false)) {
        case SelectAction::Open:
            SelectToggleOpen(self, cx);
            return;
        case SelectAction::Dismiss:
            self->open = false;
            self->list.selected = -1;
            Notify(cx);
            return;
        case SelectAction::Confirm:
            if (self->list.selected >= 0 && self->list.selected < self->matches
                                                                      .len) {
                // The same thing a click on the highlighted row does, down to
                // what the caller hears and whether the list closes behind
                // it — which is close_on_select's to decide, not the key's.
                OnRowClick(self, cx, nullptr, self->list.selected);
                // cx.stop_propagation(): the Enter was the select's, so it
                // must not also reach the focused trigger and reopen what it
                // just closed.
                if (cx->win) {
                    cx->win->eatReturn = true;
                }
            }
            return;
        case SelectAction::None:
            break;
    }
    // Not the select's — a closed one and escape, which is Rust propagating
    // so whatever encloses it can use the key.
    const_cast<ActionEvent*>(ev)->propagate = true;
}

// `.key_context(CONTEXT)` and the handlers under it. A disabled select never
// declares it, which is Rust's every-handler-propagates-when-disabled.
void SelectBindKeys(Ctx* cx, El* root, Entity<SearchableListState> state) {
    if (!cx || !root || !state.IsValid()) {
        return;
    }
    SelectInitKeys();
    Listener onAction = ListenTo(state, &SearchableListState::OnAction);
    root->KeyContext(SelectContext())
        ->OnAction(action::SelectUp(), onAction)
        ->OnAction(action::SelectDown(), onAction)
        ->OnAction(action::Confirm(), onAction)
        ->OnAction(action::Cancel(), onAction);
}

void SearchableListState::OnListAction(SearchableListState* self, Ctx* cx,
                                       const ActionEvent* ev) {
    if (!self) {
        return;
    }
    ListKeyAction act = ListActionOf(ev->action, ev->arg);
    if (act.action == ListAction::None) {
        const_cast<ActionEvent*>(ev)->propagate = true;
        return;
    }
    if (act.action == ListAction::Confirm) {
        if (self->list.selected >= 0 && self->list.selected < self->matches
                                                                  .len) {
            OnRowClick(self, cx, nullptr, self->list.selected);
        }
        return;
    }
    ListPerform(&self->list, cx, act.action, act.secondary);
    Notify(cx);
}

} // namespace component
} // namespace gpui
