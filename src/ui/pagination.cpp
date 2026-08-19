#include "ui/pagination.h"
#include "ui/button.h"

namespace gpui {

namespace component {

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

// crates/base/src/pagination.rs calculate_items: the first and last page
// always show, with the window around the current one and an ellipsis for
// each gap.
struct PageItem {
    int page = 0; // 0 for an ellipsis
    int from = 0; // the range an ellipsis stands for
    int to = 0;
};

static int PageItems(int current, int total, int maxVisible, PageItem* out,
                     int cap) {
    int n = 0;
    if (total <= 1) {
        return 0;
    }
    if (maxVisible < 5) {
        maxVisible = 5;
    }
    if (total <= maxVisible) {
        for (int i = 1; i <= total && n < cap; i++) {
            out[n].page = i;
            n++;
        }
        return n;
    }
    out[n].page = 1;
    n++;
    int side = (maxVisible - 3) / 2;
    int start = current <= side + 1          ? 2
                : current > total - side - 1 ? total - side - 1
                                             : current - side;
    if (start > 2 && n < cap) {
        out[n].page = 0;
        out[n].from = 2;
        out[n].to = start - 1;
        n++;
    }
    int end = current >= total - side ? total - 1
              : current <= side + 1   ? side + 2
                                      : current + side;
    for (int i = start; i <= end && n < cap; i++) {
        out[n].page = i;
        n++;
    }
    if (end < total - 1 && n < cap) {
        out[n].page = 0;
        out[n].from = end + 1;
        out[n].to = total - 1;
        n++;
    }
    if (n < cap) {
        out[n].page = total;
        n++;
    }
    return n;
}

El* Pagination::IntoEl() {
    Str base = id.s ? id : StrL("pagination");
    El* row = gpui::Pagination::New(cx, base)
                  ->FlexRow()
                  ->PadX(8)
                  ->PadY(8)
                  ->Gap(4)
                  ->ItemsCenter();
    // The nav buttons are ghost and compact; only the icon shows when compact.
    bool hasPrev = page > 1 && !disabled;
    bool hasNext = page < total && !disabled;
    Button* prev = Button::New(cx, StrDup(a, fmt("%s-prev", base)))
                       ->Ghost()
                       ->Compact()
                       ->WithSize(size)
                       ->Disabled(!hasPrev);
    Button* next = Button::New(cx, StrDup(a, fmt("%s-next", base)))
                       ->Ghost()
                       ->Compact()
                       ->WithSize(size)
                       ->Disabled(!hasNext);
    if (compact) {
        prev->Icon(IconName::ChevronLeft);
        next->Icon(IconName::ChevronRight);
    } else {
        prev->Icon(IconName::ChevronLeft)->Label(StrL("Previous"));
        next->Label(StrL("Next"))->Extra(IconEl(a, IconName::ChevronRight, 16));
    }
    if (hasPrev && onChange.IsValid()) {
        prev->OnClick(ListenerArg(onChange, page - 1));
    }
    if (hasNext && onChange.IsValid()) {
        next->OnClick(ListenerArg(onChange, page + 1));
    }
    row->Child(prev->IntoEl());
    if (!compact) {
        PageItem items[32];
        int n = PageItems(page, total, visiblePages, items, 32);
        for (int i = 0; i < n; i++) {
            if (items[i].page == 0) {
                row->Child(
                    Button::New(cx, StrDup(a, fmt("%s-ellipsis-%d", base, i)))
                        ->Ghost()
                        ->Compact()
                        ->WithSize(size)
                        ->Disabled(disabled)
                        ->Icon(IconName::Ellipsis)
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
            if (!selected && !disabled && onChange.IsValid()) {
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
