#include "ui/list.h"
#include "base/list_settings.h"
#include "ui/input.h"
#include "ui/skeleton.h"

namespace gpui {

namespace component {

ListItem* ListItem::New(Ctx* cx, El* child) {
    Arena* a = cx->a;
    ListItem* it = ArenaNew<ListItem>(a);
    it->a = a;
    it->cx = cx;
    it->child = child;
    return it;
}
ListItem* ListItem::Selected(bool v) {
    selected = v;
    return this;
}
ListItem* ListItem::SecondarySelected(bool v) {
    secondarySelected = v;
    return this;
}
ListItem* ListItem::Confirmed(bool v) {
    confirmed = v;
    return this;
}
ListItem* ListItem::Disabled(bool v) {
    disabled = v;
    return this;
}

El* ListItem::IntoEl(Str id, Listener onClick, Listener onMouseDown) {
    const Theme& th = cx->theme();
    El* row = Div(a)
                  ->FlexRow()
                  ->W(kFill)
                  ->PadX(8)
                  ->PadY(4)
                  ->Gap(8)
                  ->ItemsCenter()
                  ->JustifyBetween()
                  ->Radius(th.radius);
    if (!disabled) {
        row->HoverBg(th.tokens.accent);
    }
    if (selected || secondarySelected) {
        // list_item.rs: the selection takes the active highlight when the
        // setting is on — the list.active tint, ruled with list.active.border
        // — and plain `accent` when it is off. A row a right press marked is
        // outlined rather than filled, so it is not mistaken for the
        // selection.
        ListActiveStyle st =
            ListActiveStyleOf(th.tokens.listActive, th.listActiveBorder,
                              th.tokens.accent, selected);
        if (!secondarySelected) {
            row->Bg(st.bg);
        }
        if (st.hasBorder) {
            row->Child(ListActiveOverlay(a, st.border, th.radius));
        } else if (secondarySelected) {
            row->Border(1, th.border);
        }
    }
    if (child) {
        row->Child(child);
    }
    if (confirmed) {
        row->Child(IconEl(a, IconName::Check, 16)->Fg(th.foreground));
    }
    if (!disabled) {
        BindClick(row, id, onClick);
        if (onMouseDown.IsValid()) {
            row->OnMouseDown(onMouseDown);
        }
    }
    return row;
}

List* List::New(Ctx* cx, Str id, Entity<ListState> state) {
    Arena* a = cx->a;
    List* l = ArenaNew<List>(a);
    l->a = a;
    l->cx = cx;
    l->id = id;
    l->state = state;
    return l;
}
List* List::Sections(const int* counts, int n) {
    ListState* s = state.Get(cx);
    if (s) {
        ListSetSections(s, counts, n, header != nullptr, footer != nullptr);
    }
    return this;
}
List* List::Count(int n) {
    ListState* s = state.Get(cx);
    if (s) {
        ListSetCount(s, n);
    }
    return this;
}
List* List::Items(void* d, ListItem* (*fn)(Ctx*, void*, int, int, int)) {
    data = d;
    item = fn;
    return this;
}
List* List::Headers(El* (*headerFn)(Ctx*, void*, int),
                    El* (*footerFn)(Ctx*, void*, int)) {
    header = headerFn;
    footer = footerFn;
    // The flattening depends on whether there are headers and footers at all,
    // so a list that says so after its sections says it again.
    ListState* s = state.Get(cx);
    if (s) {
        s->sectionHeaders = header != nullptr;
        s->sectionFooters = footer != nullptr;
    }
    return this;
}
ListItem* ListSeparatorItem(Ctx* cx, El* child) {
    return ListItem::New(cx, child)->Disabled(true);
}

List* List::Searchable(InputState* s, Listener onFocus) {
    search = s;
    onSearchFocus = onFocus;
    return this;
}

List* List::Loading(El* e) {
    loading = e;
    return this;
}

List* List::Initial(El* e) {
    initial = e;
    return this;
}
List* List::Empty(El* e) {
    empty = e;
    return this;
}
List* List::H(float px) {
    h = px;
    return this;
}

// list/loading.rs: three placeholder rows, each a wide bar over a narrower
// secondary one. Rust builds them out of ListItem so they carry a row's own
// padding; the shape is the same either way.
El* ListLoadingView(Ctx* cx, float h) {
    Arena* a = cx->a;
    El* body = Div(a)->FlexCol()->W(kFill)->PadY(10)->Gap(12);
    if (h > 0) {
        body->H(h);
    }
    for (int i = 0; i < 3; i++) {
        body->Child(
            Div(a)
                ->FlexCol()
                ->W(kFill)
                ->PadX(8)
                ->Gap(6)
                // max_w_full: the bars keep their own widths and a
                // list narrower than they are clips them rather than
                // letting them hang over its edge.
                ->ItemsStart()
                ->ClipX()
                ->Child(Skeleton::New(cx)->W(192)->H(20)->IntoEl())
                ->Child(
                    Skeleton::New(cx)->Secondary()->W(256)->H(12)->IntoEl()));
    }
    return body;
}

// render_empty: an Inbox icon in muted_foreground at 60%, centred in what the
// list would have filled.
static El* DefaultEmpty(Ctx* cx, float h) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    return Div(a)
        ->FlexCol()
        ->W(kFill)
        ->H(h)
        ->ItemsCenter()
        ->JustifyCenter()
        ->Child(IconEl(a, IconName::Inbox, 48)
                    ->Fg(RgbaOpacity(th.mutedFg, 0.6f)));
}

El* List::IntoEl() {
    const Theme& th = cx->theme();
    ListState* s = state.Get(cx);
    if (s) {
        s->self = state.id;
    }

    El* root =
        Div(a)->FlexCol()->W(kFill)->Pad(8)->Gap(4)->Radius(th.radius)->Border(
            1, th.border);
    if (search) {
        // The search field is part of the list in Rust too, above the rows.
        El* searchRow =
            Div(a)->FlexRow()->W(kFill)->H(32)->PadX(8)->Gap(8)->ItemsCenter();
        searchRow->Child(IconEl(a, IconName::Search, 16)->Fg(th.mutedFg));
        // InputState::new(..).placeholder(t!("List.search_placeholder")),
        // which is "Search..." in the locale this tree ships. Rust sets it on
        // the state when the list makes it, so a caller that gave a field of
        // its own with a placeholder already on it keeps that one.
        if (!search->placeholder.s) {
            InputSetPlaceholder(search, StrL("Search..."));
        }
        searchRow->Child(Div(a)->Flex1()->Child(
            Input::New(cx, StrDup(a, fmt("%s-search", id)), search)
                ->Appearance(false)
                ->Cleanable(true)
                ->OnFocus(onSearchFocus)
                ->IntoEl()));
        root->Child(searchRow);
    }
    if (!s) {
        return root;
    }
    // The height the list was laid out at, which is what scroll_to_item and
    // the visible range are worked out against.
    s->viewportH = h;

    if (s->loading) {
        root->Child(loading ? loading : ListLoadingView(cx, h));
        return root;
    }
    // render_initial: what the list shows before anything has been searched
    // for. Rust asks for it only while the query field is empty, which is the
    // one place it could be reached from.
    if (initial && (!search || search->text.len == 0)) {
        root->Child(initial);
        return root;
    }
    if (s->count == 0) {
        root->Child(empty ? empty : DefaultEmpty(cx, h));
        return root;
    }

    int total = ListRowCount(s);
    VirtualRange range = VirtualListVisibleRows(total, s->rowH, s->scrollY, h);
    El* body = Div(a)
                   ->FlexCol()
                   ->W(kFill)
                   ->H(h)
                   ->ClipY()
                   ->ScrollY(s->scrollY)
                   ->ScrollId(HashClickId(id))
                   ->OnScroll(ListenTo(state, &ListState::OnScroll));
    if (range.first > 0) {
        body->Child(Div(a)->W(kFill)->H((float)range.first * s->rowH));
    }
    Listener click = ListenTo(state, &ListState::OnRowClick, 0);
    Listener down = ListenTo(state, &ListState::OnRowMouseDown, 0);
    for (int r = range.first; r < range.end; r++) {
        ListRow row = ListRowAt(s, r);
        El* el = nullptr;
        if (row.kind == ListRowKind::SectionHeader) {
            el = header ? header(cx, data, row.section) : nullptr;
        } else if (row.kind == ListRowKind::SectionFooter) {
            el = footer ? footer(cx, data, row.section) : nullptr;
        } else if (item) {
            ListItem* it = item(cx, data, row.section, row.row, row.entry);
            if (it) {
                it->selected = s->selectable && s->selected == row.entry;
                it->secondarySelected = s->rightClicked == row.entry;
                // Each row names the state and carries its own index, which
                // is what Rust's per-row closure captures. The name is the
                // row's IndexPath, the way `impl From<IndexPath> for
                // ElementId` spells it, so it stays the same when a section
                // above it grows or shrinks and the flat index shifts.
                el = it->IntoEl(
                    StrDup(a, fmt("%s-%s", id, IndexPathIdStr(a, row.Path()))),
                    ListenerArg(click, row.entry),
                    ListenerArg(down, row.entry));
            }
        }
        // Every row is the same height, which is what uniform_list asks for
        // and what lets the two spacers stand in for the rest.
        El* slot = Div(a)->FlexCol()->W(kFill)->H(s->rowH);
        if (el) {
            slot->Child(el);
        }
        body->Child(slot);
    }
    if (range.end < total) {
        body->Child(Div(a)->W(kFill)->H((float)(total - range.end) * s->rowH));
    }
    root->Child(body);

    // load_more: coming within the threshold of the end asks the caller for
    // more rows. Rust runs it as a background task; here it is a listener the
    // caller answers on the next frame.
    if (ListShouldLoadMore(s, range.end) && s->onLoadMore.IsValid()) {
        ListEvent ev = {ListEventKind::Select, s->count, false};
        ListenerCall(cx->app, cx->win, s->onLoadMore, &ev);
    }
    // list.rs declares the "List" context on the element it tracks focus
    // on; the root is focusable here for the same reason — clicking a row
    // focuses the row, which is inside it, and Tab reaches the list itself.
    // list.rs `self.focus_handle(cx).focus(window, cx)` on a row press.
    root->FocusId(HashClickId(id))->FocusRing(false)->FocusOnPress();
    ListBindKeys(cx, root, state);
    return root;
}

} // namespace component
} // namespace gpui
