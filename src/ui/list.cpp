#include "ui/list.h"
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
        row->HoverBg(th.accent);
    }
    if (selected) {
        row->Bg(th.accent);
    } else if (secondarySelected) {
        // secondary_selected: the row a right press marked, outlined rather
        // than filled, so it is not mistaken for the selection.
        row->Border(1, th.border);
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
List* List::Searchable(InputState* s, Listener onFocus) {
    search = s;
    onSearchFocus = onFocus;
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

    El* root =
        Div(a)->FlexCol()->W(kFill)->Pad(8)->Gap(4)->Radius(th.radius)->Border(
            1, th.border);
    if (search) {
        // The search field is part of the list in Rust too, above the rows.
        El* searchRow =
            Div(a)->FlexRow()->W(kFill)->H(32)->PadX(8)->Gap(8)->ItemsCenter();
        searchRow->Child(IconEl(a, IconName::Search, 16)->Fg(th.mutedFg));
        searchRow->Child(Div(a)->Grow()->Child(
            Input::New(cx, StrDup(a, fmt("%s-search", id)), search)
                ->Appearance(false)
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
        // The delegate's loading state: skeleton rows in place of the list.
        El* body = Div(a)->FlexCol()->W(kFill)->H(h);
        for (int i = 0; i < 5; i++) {
            body->Child(Div(a)->W(kFill)->PadX(8)->PadY(6)->Child(
                Skeleton::New(cx)->W(kFill)->H(16)->IntoEl()));
        }
        root->Child(body);
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
                // is what Rust's per-row closure captures.
                el = it->IntoEl(StrDup(a, fmt("%s-row-%d", id, row.entry)),
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
    return root;
}

} // namespace component
} // namespace gpui
