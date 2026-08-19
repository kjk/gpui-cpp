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
List* List::Section(El* header, El* footer) {
    if (nSections < 8) {
        sections[nSections].header = header;
        sections[nSections].footer = footer;
        nSections++;
    }
    return this;
}
List* List::Item(ListItem* item) {
    if (nSections == 0) {
        // A list with no section of its own still has one to put rows in.
        Section(nullptr);
    }
    ListSection& sec = sections[nSections - 1];
    if (sec.n < 64 && item) {
        sec.rows[sec.n++] = item;
        nRows++;
    }
    return this;
}
List* List::Searchable(InputState* s, Listener onFocus) {
    search = s;
    onSearchFocus = onFocus;
    return this;
}
List* List::Loading(bool v) {
    loading = v;
    return this;
}
List* List::MaxH(float px) {
    maxH = px;
    return this;
}

El* List::IntoEl() {
    const Theme& th = cx->theme();
    ListState* s = state.Get(cx);
    if (s) {
        // The row count is the caller's every frame, which is what keeps the
        // arrow keys inside the rows there actually are.
        s->count = nRows;
    }

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

    El* body = Div(a)->FlexCol()->W(kFill);
    if (maxH > 0) {
        body->MaxH(maxH);
    }
    if (loading) {
        // The delegate's loading state: skeleton rows in place of the list.
        for (int i = 0; i < 5; i++) {
            body->Child(Div(a)->W(kFill)->PadX(8)->PadY(6)->Child(
                Skeleton::New(cx)->W(kFill)->H(16)->IntoEl()));
        }
        root->Child(body);
        return root;
    }

    Listener click = ListenTo(state, &ListState::OnRowClick, 0);
    Listener down = ListenTo(state, &ListState::OnRowMouseDown, 0);
    int ix = 0;
    for (int si = 0; si < nSections; si++) {
        const ListSection& sec = sections[si];
        if (sec.header) {
            body->Child(sec.header);
        }
        for (int i = 0; i < sec.n; i++) {
            ListItem* it = sec.rows[i];
            if (s) {
                it->selected = s->selectable && s->selected == ix;
                it->secondarySelected = s->rightClicked == ix;
            }
            // Each row names the state and carries its own index, which is
            // what Rust's per-row closure captures.
            body->Child(it->IntoEl(StrDup(a, fmt("%s-row-%d", id, ix)),
                                   ListenerArg(click, ix),
                                   ListenerArg(down, ix)));
            ix++;
        }
        if (sec.footer) {
            body->Child(sec.footer);
        }
    }
    root->Child(body);
    return root;
}

} // namespace component
} // namespace gpui
