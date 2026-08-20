#include "base/popup_menu.h"

namespace gpui {

PopupMenuAction PopupMenuActionForKey(int key, Side side) {
    switch (key) {
        case KeyUp:
            return PopupMenuAction::SelectPrev;
        case KeyDown:
            return PopupMenuAction::SelectNext;
        case KeyReturn:
            return PopupMenuAction::Confirm;
        case KeyEscape:
            return PopupMenuAction::Cancel;
        case KeyLeft:
            // A menu whose submenus open to the right is stepped out of with
            // Left; one that opens to the left reaches in with it.
            return SideIsLeft(side) ? PopupMenuAction::OpenSubmenu
                                    : PopupMenuAction::CloseSubmenu;
        case KeyRight:
            return SideIsLeft(side) ? PopupMenuAction::CloseSubmenu
                                    : PopupMenuAction::OpenSubmenu;
        default:
            return PopupMenuAction::None;
    }
}

int PopupMenuNextIndex(const bool* clickable, int n, int selected) {
    if (n <= 0) {
        return -1;
    }
    if (selected < 0) {
        // set_selected_index(0): Rust takes the first row whether or not it
        // is clickable.
        return 0;
    }
    for (int i = selected + 1; i < n; i++) {
        if (clickable[i]) {
            return i;
        }
    }
    return 0;
}

int PopupMenuPrevIndex(const bool* clickable, int n, int selected) {
    if (n <= 0) {
        return -1;
    }
    int ix = selected < 0 ? 0 : selected;
    for (int i = ix - 1; i >= 0; i--) {
        if (clickable[i]) {
            return i;
        }
    }
    // No clickable row before it: the last clickable one, or 0 if there is
    // none at all.
    for (int i = n - 1; i >= 0; i--) {
        if (clickable[i]) {
            return i;
        }
    }
    return 0;
}

void PopupMenuOpen(PopupMenuState* s, Ctx* cx) {
    s->open = true;
    s->selected = -1;
    s->openSubmenu = -1;
    Notify(cx);
}

void PopupMenuDismiss(PopupMenuState* s, Ctx* cx) {
    s->open = false;
    s->selected = -1;
    s->openSubmenu = -1;
    Notify(cx);
}

void PopupMenuDismissAll(PopupMenuState* s, Ctx* cx) {
    Entity<PopupMenuState> parent = s->parent;
    PopupMenuDismiss(s, cx);
    while (parent.IsValid()) {
        PopupMenuState* p = parent.Get(cx);
        if (!p) {
            break;
        }
        parent = p->parent;
        PopupMenuDismiss(p, cx);
    }
}

void PopupMenuConfirm(PopupMenuState* s, Ctx* cx, int ix) {
    if (s->onConfirm.IsValid()) {
        ClickEvent ev = {};
        ListenerCall(cx->app, cx->win, ListenerFill(s->onConfirm, ix), &ev);
    }
    // The item ran, so this menu and every parent go. Rust spells this
    // dismiss_all; without it, choosing a nested item leaves the top-level
    // dropdown behind.
    PopupMenuDismissAll(s, cx);
}

void PopupMenuPerform(PopupMenuState* s, Ctx* cx, PopupMenuAction act,
                      const bool* clickable, const bool* hasSubmenu, int n) {
    switch (act) {
        case PopupMenuAction::SelectPrev:
            s->selected = PopupMenuPrevIndex(clickable, n, s->selected);
            s->openSubmenu = -1;
            Notify(cx);
            break;
        case PopupMenuAction::SelectNext:
            s->selected = PopupMenuNextIndex(clickable, n, s->selected);
            s->openSubmenu = -1;
            Notify(cx);
            break;
        case PopupMenuAction::Confirm:
            if (s->selected >= 0 && s->selected < n && clickable[s->selected]) {
                if (hasSubmenu && hasSubmenu[s->selected]) {
                    // Enter on a submenu row opens it rather than running it.
                    s->openSubmenu = s->selected;
                    Notify(cx);
                } else {
                    PopupMenuConfirm(s, cx, s->selected);
                }
            }
            break;
        case PopupMenuAction::Cancel:
            // Escape closes the submenu first, and the menu itself only once
            // there is no submenu left to close.
            if (s->openSubmenu >= 0) {
                s->openSubmenu = -1;
                Notify(cx);
            } else {
                PopupMenuDismiss(s, cx);
            }
            break;
        case PopupMenuAction::OpenSubmenu:
            if (s->selected >= 0 && s->selected < n && hasSubmenu &&
                hasSubmenu[s->selected]) {
                s->openSubmenu = s->selected;
                Notify(cx);
            }
            break;
        case PopupMenuAction::CloseSubmenu:
            if (s->openSubmenu >= 0) {
                s->openSubmenu = -1;
                Notify(cx);
            }
            break;
        default:
            break;
    }
}

void PopupMenuState::OnItemClick(PopupMenuState* self, Ctx* cx,
                                 const ClickEvent*, intptr_t ix) {
    self->selected = (int)ix;
    PopupMenuConfirm(self, cx, (int)ix);
}

void PopupMenuState::OnTriggerClick(PopupMenuState* self, Ctx* cx,
                                    const ClickEvent*) {
    if (self->open) {
        PopupMenuDismiss(self, cx);
    } else {
        self->x = 0;
        self->y = 0;
        PopupMenuOpen(self, cx);
    }
}

void PopupMenuState::OnContextDown(PopupMenuState* self, Ctx* cx,
                                   const MouseDownEvent* ev) {
    // on_mouse_down(MouseButton::Right): the menu opens where the pointer is,
    // inside the element that owns it.
    if (ev->button != MouseButton::Right) {
        return;
    }
    self->x = ev->x - ev->el.x;
    self->y = ev->y - ev->el.y;
    PopupMenuOpen(self, cx);
}

void PopupMenuState::OnItemHover(PopupMenuState* self, Ctx* cx,
                                 const HoverEvent* ev, intptr_t ix) {
    // The hovered row is the selected one; leaving it deselects, unless it is
    // a submenu row, which stays selected while the pointer travels into the
    // submenu it opened.
    if (ev->hovered) {
        self->selected = (int)ix;
        self->openSubmenu = -1;
    } else if (self->selected == (int)ix && self->openSubmenu != (int)ix) {
        self->selected = -1;
    }
    Notify(cx);
}

void PopupMenuState::OnSubmenuClick(PopupMenuState* self, Ctx* cx,
                                    const ClickEvent*, intptr_t ix) {
    self->selected = (int)ix;
    self->openSubmenu = (int)ix;
    Notify(cx);
}

void PopupMenuState::OnSubmenuHover(PopupMenuState* self, Ctx* cx,
                                    const HoverEvent* ev, intptr_t ix) {
    // Rust renders a submenu as soon as its row becomes selected. Keep it
    // selected while the pointer crosses from the row into the child menu.
    if (ev->hovered) {
        self->selected = (int)ix;
        self->openSubmenu = (int)ix;
        Notify(cx);
    }
}

void PopupMenuState::OnScroll(PopupMenuState* self, Ctx* cx,
                              const ScrollEvent* ev) {
    self->scrollY = ev->offsetY;
    Notify(cx);
}

} // namespace gpui
