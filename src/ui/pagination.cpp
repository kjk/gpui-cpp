#include "ui/pagination.h"
#include "ui/button.h"
#include "ui/menu.h"

namespace gpui {

namespace component {

void PaginationMenuState::OnItem(PaginationMenuState* self, Ctx* cx,
                                 const ClickEvent* ev, intptr_t ix) {
    if (!self->onChange.IsValid()) {
        return;
    }
    ListenerCall(cx->app, cx->win,
                 ListenerFill(self->onChange, self->firstPage + (int)ix), ev);
}

Pagination* Pagination::New(Ctx* cx, int page, int total) {
    Arena* a = cx->a;
    Pagination* p = ArenaNew<Pagination>(a);
    p->a = a;
    p->cx = cx;
    p->page = page;
    p->total = total;
    return p;
}
Pagination* Pagination::Id(Str s) {
    id = s;
    return this;
}
Pagination* Pagination::VisiblePages(int n) {
    visiblePages = n;
    return this;
}
Pagination* Pagination::Compact(bool v) {
    compact = v;
    return this;
}
Pagination* Pagination::Disabled(bool v) {
    disabled = v;
    return this;
}
Pagination* Pagination::WithSize(UiSize s) {
    size = s;
    return this;
}
Pagination* Pagination::OnChange(Listener fn) {
    onChange = fn;
    return this;
}

El* Pagination::IntoEl() {
    Str base = id.s ? id : StrL("pagination");
    // gpui_base::PaginationState owns the clamping, the bounds and the
    // ellipsis window; this layer only paints what it is told.
    PaginationState st = PaginationStateNew(page, total);
    st.visiblePages = visiblePages;
    st.disabled = disabled;
    El* row = gpui::Pagination::New(cx, base)
                  ->FlexRow()
                  ->PadX(8)
                  ->PadY(8)
                  ->Gap(4)
                  ->ItemsCenter();
    // The nav buttons are ghost and compact; only the icon shows when compact.
    int prevPage = PaginationPrevPage(&st);
    int nextPage = PaginationNextPage(&st);
    bool hasPrev = prevPage != 0;
    bool hasNext = nextPage != 0;
    Button* prev = Button::New(cx, StrDup(a, fmt("%s-prev", base)))
                       ->Ghost()
                       ->Compact()
                       ->WithSize(size)
                       ->Disabled(disabled || !hasPrev);
    Button* next = Button::New(cx, StrDup(a, fmt("%s-next", base)))
                       ->Ghost()
                       ->Compact()
                       ->WithSize(size)
                       ->Disabled(disabled || !hasNext);
    if (compact) {
        prev->Icon(IconName::ChevronLeft);
        next->Icon(IconName::ChevronRight);
    } else {
        // The nav content is a row of [label, icon], reversed for Previous:
        // the chevron leads the word going back and follows it going on.
        prev->Icon(IconName::ChevronLeft)->Label(StrL("Previous"));
        next->Label(StrL("Next"))->IconRight(IconName::ChevronRight);
    }
    if (hasPrev && onChange.IsValid()) {
        prev->OnClick(ListenerArg(onChange, prevPage));
    }
    if (hasNext && onChange.IsValid()) {
        next->OnClick(ListenerArg(onChange, nextPage));
    }
    row->Child(prev->IntoEl());
    if (!compact) {
        PaginationItem items[32];
        int n = PaginationItems(&st, items, 32);
        for (int i = 0; i < n; i++) {
            if (items[i].page == 0) {
                // The ellipsis is a dropdown over the pages it hid, so a jump
                // into the middle of a long run does not need the arrows.
                Str menuId = StrDup(a, fmt("%s-ellipsis-%d", base, i));
                El* trigger = Button::New(cx, menuId)
                                  ->Ghost()
                                  ->Compact()
                                  ->WithSize(size)
                                  ->Disabled(disabled)
                                  ->Icon(IconName::Ellipsis)
                                  ->IntoEl();
                if (disabled || !onChange.IsValid()) {
                    row->Child(trigger);
                    continue;
                }
                PopupMenu* menu = PopupMenu::New(cx, menuId)
                                      ->MinW(55)
                                      ->MaxH(240)
                                      ->Scrollable();
                for (int p = items[i].from; p <= items[i].to; p++) {
                    menu->MenuWithCheck(StrDup(a, fmt("%d", p)), p == page);
                }
                Entity<PaginationMenuState> ment =
                    KeyedEntity<PaginationMenuState>(
                        cx, KeyedKey(HashClickId(menuId),
                                     HashClickId(StrL("pagination-menu"))));
                if (PaginationMenuState* ms = ment.Get(cx)) {
                    ms->firstPage = items[i].from;
                    ms->onChange = onChange;
                }
                if (PopupMenuState* ps = menu->state.Get(cx)) {
                    ps->onConfirm =
                        ListenTo(ment, &PaginationMenuState::OnItem);
                }
                row->Child(DropdownMenu::New(cx, menuId)
                               ->Trigger(trigger)
                               ->Menu(menu)
                               ->IntoEl());
                continue;
            }
            bool selected = items[i].page == page;
            Button* b =
                Button::New(cx,
                            StrDup(a, fmt("%s-page-%d", base, items[i].page)))
                    ->Label(StrDup(a, fmt("%d", items[i].page)))
                    ->Compact()
                    ->WithSize(size)
                    ->Disabled(disabled);
            if (selected) {
                b->Outline();
            } else {
                b->Ghost();
            }
            if (onChange.IsValid() &&
                PaginationCanRequest(&st, items[i].page)) {
                b->OnClick(ListenerArg(onChange, items[i].page));
            }
            row->Child(b->IntoEl());
        }
    }
    row->Child(next->IntoEl());
    return row;
}

} // namespace component
} // namespace gpui
