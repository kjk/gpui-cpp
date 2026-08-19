#include "base/list.h"

namespace gpui {

ListAction ListActionForKey(int key) {
    switch (key) {
        case KeyUp:
            return ListAction::SelectPrev;
        case KeyDown:
            return ListAction::SelectNext;
        case KeyReturn:
            return ListAction::Confirm;
        case KeyEscape:
            return ListAction::Cancel;
        default:
            return ListAction::None;
    }
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

static void ListEmit(ListState* s, Ctx* cx, ListEventKind kind, int index,
                     bool secondary) {
    if (!s->onEvent.IsValid()) {
        return;
    }
    ListEvent ev = {kind, index, secondary};
    ListenerCall(cx->app, cx->win, s->onEvent, &ev);
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
            }
            break;
        case ListAction::SelectNext:
            if (s->count > 0) {
                ListSelect(s, cx, ListNextIndex(s));
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

void ListState::OnRowMouseDown(ListState* self, Ctx* cx,
                               const MouseDownEvent* ev, intptr_t ix) {
    // on_mouse_down(MouseButton::Right): the row under a secondary press is
    // marked, and the left button is left to the click path above.
    if (ev->button == MouseButton::Right) {
        ListRightClickRow(self, cx, (int)ix);
    }
}

} // namespace gpui
