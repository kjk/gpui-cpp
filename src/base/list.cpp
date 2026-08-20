#include "base/list.h"
#include "base/actions.h"
#include "gpui/keymap.h"

namespace gpui {

Str ListContext() {
    return StrL("List");
}

void ListInitKeys() {
    static uint32_t bound = 0;
    if (bound == KeymapGeneration()) {
        return;
    }
    bound = KeymapGeneration();
    const char* ctx = "List";
    KeyBinding bindings[] = {
        {"escape", action::Cancel(), ctx},
        {"enter", action::Confirm(), ctx},
        {"secondary-enter", action::ConfirmSecondary(), ctx},
        {"up", action::SelectUp(), ctx},
        {"down", action::SelectDown(), ctx},
    };
    KeymapBind(bindings, (int)(sizeof(bindings) / sizeof(bindings[0])));
}

ListKeyAction ListActionOf(uint32_t id) {
    ListKeyAction out;
    if (id == action::SelectUp()) {
        out.action = ListAction::SelectPrev;
    } else if (id == action::SelectDown()) {
        out.action = ListAction::SelectNext;
    } else if (id == action::Confirm()) {
        out.action = ListAction::Confirm;
    } else if (id == action::ConfirmSecondary()) {
        // The second binding of the same Rust action, carrying the flag its
        // payload would have.
        out.action = ListAction::Confirm;
        out.secondary = true;
    } else if (id == action::Cancel()) {
        out.action = ListAction::Cancel;
    }
    return out;
}

int ListNextIndex(const ListState* s) {
    if (s->count <= 0) {
        return -1;
    }
    if (s->selected < 0) {
        return 0;
    }
    int next = s->selected + 1;
    return next < s->count ? next : 0;
}

int ListPrevIndex(const ListState* s) {
    if (s->count <= 0) {
        return -1;
    }
    if (s->selected < 0) {
        return s->count - 1;
    }
    int prev = s->selected - 1;
    return prev >= 0 ? prev : s->count - 1;
}

void ListSetSections(ListState* s, const int* counts, int n, bool headers,
                     bool footers) {
    s->sectionCounts.Clear();
    s->count = 0;
    for (int i = 0; i < n; i++) {
        int c = counts[i] < 0 ? 0 : counts[i];
        s->sectionCounts.Append(c);
        s->count += c;
    }
    s->sectionHeaders = headers;
    s->sectionFooters = footers;
}

void ListSetCount(ListState* s, int count) {
    int one = count < 0 ? 0 : count;
    ListSetSections(s, &one, 1, false, false);
}

// How many flattened rows a section contributes. An empty one contributes
// nothing — Rust skips its header and footer with it.
static int SectionRows(const ListState* s, int section) {
    int items = s->sectionCounts[section];
    if (items <= 0) {
        return 0;
    }
    return items + (s->sectionHeaders ? 1 : 0) + (s->sectionFooters ? 1 : 0);
}

int ListRowCount(const ListState* s) {
    int total = 0;
    for (int i = 0; i < s->sectionCounts.len; i++) {
        total += SectionRows(s, i);
    }
    return total;
}

ListRow ListRowAt(const ListState* s, int rowIx) {
    ListRow r;
    if (rowIx < 0) {
        return r;
    }
    int at = 0;
    int entry = 0;
    for (int i = 0; i < s->sectionCounts.len; i++) {
        int rows = SectionRows(s, i);
        if (rowIx >= at + rows) {
            at += rows;
            entry += s->sectionCounts[i];
            continue;
        }
        int within = rowIx - at;
        r.section = i;
        if (s->sectionHeaders) {
            if (within == 0) {
                r.kind = ListRowKind::SectionHeader;
                return r;
            }
            within--;
        }
        if (within < s->sectionCounts[i]) {
            r.kind = ListRowKind::Entry;
            r.row = within;
            r.entry = entry + within;
            return r;
        }
        r.kind = ListRowKind::SectionFooter;
        return r;
    }
    return r;
}

int ListRowOfEntry(const ListState* s, int entry) {
    if (entry < 0) {
        return -1;
    }
    int at = 0;
    int seen = 0;
    for (int i = 0; i < s->sectionCounts.len; i++) {
        int items = s->sectionCounts[i];
        if (items <= 0) {
            continue;
        }
        if (entry < seen + items) {
            return at + (s->sectionHeaders ? 1 : 0) + (entry - seen);
        }
        at += SectionRows(s, i);
        seen += items;
    }
    return -1;
}

void ListScrollToItem(ListState* s, int entry, ScrollStrategy strategy) {
    if (s->viewportH <= 0) {
        return;
    }
    int row = ListRowOfEntry(s, entry);
    if (row < 0) {
        return;
    }
    s->scrollY = VirtualListScrollToRow(ListRowCount(s), s->rowH, row,
                                        s->scrollY, s->viewportH, strategy);
}

bool ListShouldLoadMore(const ListState* s, int lastVisibleRow) {
    if (!s->hasMore || s->loading) {
        return false;
    }
    return ListRowCount(s) - lastVisibleRow <= s->loadMoreThreshold;
}

// cx.emit(ListEvent::..). Rust has only the subscriber list; the state's own
// onEvent is the shorthand this port already had for the single-subscriber
// case, so an event goes to it and to everything that has subscribed.
static void ListEmit(ListState* s, Ctx* cx, ListEventKind kind, int index,
                     bool secondary) {
    ListEvent ev = {kind, index, secondary};
    if (s->onEvent.IsValid()) {
        ListenerCall(cx->app, cx->win, s->onEvent, &ev);
    }
    EntityEmit(cx->app, cx->win, s->self, &ev);
}

// select_item: a list that is not selectable moves nothing.
static void ListSelect(ListState* s, Ctx* cx, int ix) {
    if (!s->selectable) {
        return;
    }
    s->selected = ix;
    Notify(cx);
    ListEmit(s, cx, ListEventKind::Select, ix, false);
}

void ListPerform(ListState* s, Ctx* cx, ListAction act, bool secondary) {
    switch (act) {
        case ListAction::SelectPrev:
            if (s->count > 0) {
                ListSelect(s, cx, ListPrevIndex(s));
                // scroll_to_selected_item: a selection that moved off the
                // bottom of the viewport brings the viewport with it.
                ListScrollToItem(s, s->selected, ScrollStrategy::Top);
            }
            break;
        case ListAction::SelectNext:
            if (s->count > 0) {
                ListSelect(s, cx, ListNextIndex(s));
                ListScrollToItem(s, s->selected, ScrollStrategy::Bottom);
            }
            break;
        case ListAction::Confirm:
            // on_action_confirm: nothing to confirm with no rows and no
            // selection.
            if (s->count > 0 && s->selected >= 0) {
                Notify(cx);
                ListEmit(s, cx, ListEventKind::Confirm, s->selected, secondary);
            }
            break;
        case ListAction::Cancel:
            if (s->resetOnCancel) {
                s->selected = -1;
            }
            Notify(cx);
            ListEmit(s, cx, ListEventKind::Cancel, s->selected, false);
            break;
        default:
            break;
    }
}

void ListClickRow(ListState* s, Ctx* cx, int ix, bool secondary) {
    if (!s->selectable) {
        return;
    }
    s->rightClicked = -1;
    s->selected = ix;
    Notify(cx);
    ListEmit(s, cx, ListEventKind::Confirm, ix, secondary);
}

void ListRightClickRow(ListState* s, Ctx* cx, int ix) {
    s->rightClicked = ix;
    Notify(cx);
}

void ListState::OnRowClick(ListState* self, Ctx* cx, const ClickEvent* ev,
                           intptr_t ix) {
    ListClickRow(self, cx, (int)ix, ev->modifiers.Secondary());
}

void ListState::OnScroll(ListState* self, Ctx* cx, const ScrollEvent* ev) {
    self->scrollY = ev->offsetY;
    Notify(cx);
}

void ListState::OnRowMouseDown(ListState* self, Ctx* cx,
                               const MouseDownEvent* ev, intptr_t ix) {
    // on_mouse_down(MouseButton::Right): the row under a secondary press is
    // marked, and the left button is left to the click path above.
    if (ev->button == MouseButton::Right) {
        ListRightClickRow(self, cx, (int)ix);
    }
}

void ListOnAction(ListState* self, Ctx* cx, const ActionEvent* ev) {
    if (!self) {
        return;
    }
    ListKeyAction act = ListActionOf(ev->action);
    if (act.action == ListAction::None) {
        const_cast<ActionEvent*>(ev)->propagate = true;
        return;
    }
    ListPerform(self, cx, act.action, act.secondary);
}

void ListBindKeys(Ctx* cx, El* root, Entity<ListState> state) {
    if (!cx || !root || !state.IsValid()) {
        return;
    }
    ListInitKeys();
    Listener onAction = ListenTo(state, &ListOnAction);
    root->KeyContext(ListContext())
        ->OnAction(action::Cancel(), onAction)
        ->OnAction(action::Confirm(), onAction)
        ->OnAction(action::ConfirmSecondary(), onAction)
        ->OnAction(action::SelectUp(), onAction)
        ->OnAction(action::SelectDown(), onAction);
}

} // namespace gpui
