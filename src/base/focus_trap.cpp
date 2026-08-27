#include "base/focus_trap.h"

namespace gpui {

void FocusTrapInit(App* app) {
    // Membership is frame-local in Window::focusEls, so unlike Rust's weak
    // handle map there is no global state to initialize or clean up.
    (void)app;
}

El* FocusTrapContainer::New(Ctx* cx, Str id, FocusHandle focus, El* child) {
    if (!cx || !child || !focus.IsValid()) {
        return child;
    }
    return child->Id(id)->TrackFocus(focus)->TrapId(focus.id);
}

int FocusTrapId(Str name) {
    return HashClickId(name);
}

int FocusTrapOf(const Window* win, int focusId) {
    if (!focusId) {
        return 0;
    }
    for (int i = 0; i < win->focusEls.len; i++) {
        if (win->focusEls[i].id == focusId) {
            return win->focusEls[i].trapId;
        }
    }
    return 0;
}

int FocusTrapActive(const Window* win) {
    return FocusTrapOf(win, win->focusId);
}

int FocusTrapTab(Window* win, bool backward) {
    // Rust reads the trap first and only then moves, so a Tab that would
    // leave the container comes back to its other end instead. FocusNext
    // takes the trap for the same reason.
    return FocusNext(win, FocusTrapActive(win), backward);
}

bool FocusTrapEnter(Window* win, int trapId, bool backward) {
    if (!trapId) {
        return false;
    }
    int n = win->focusEls.len;
    for (int k = 0; k < n; k++) {
        int i = backward ? n - 1 - k : k;
        if (win->focusEls[i].trapId != trapId || !win->focusEls[i].tabStop) {
            continue;
        }
        if (win->focusEls[i].id == win->focusId) {
            return false;
        }
        WindowSetFocusId(win, win->focusEls[i].id);
        return true;
    }
    return false;
}

void FocusTrapArm(Window* win, int trapId, int hostFocusId) {
    if (win) {
        win->pendingTrap = trapId;
        win->pendingTrapHost = hostFocusId;
    }
}

void FocusTrapApplyPending(Window* win) {
    int trap = win->pendingTrap;
    int host = win->pendingTrapHost;
    if (!trap) {
        return;
    }
    // The open transition owns focus; later renders do not. This matches the
    // reverted dialog focus-reclaim change at upstream d5821f27.
    if (trap == win->previousTrap) {
        return;
    }
    // Focus that is already inside stays where it is — the trap keeps focus
    // in, it does not send it back to the top on every frame.
    if (FocusTrapActive(win) == trap) {
        return;
    }
    // The trap's own container first, which is where Rust puts it:
    // `track_focus(&self.focus).focus_trap(.., &self.focus)` on the dialog
    // root, and nothing focuses a control inside it. Focusing the first
    // control instead put a focus ring on a dialog's Cancel button the
    // moment it opened, which the Rust window never shows.
    if (host) {
        for (int i = 0; i < win->focusEls.len; i++) {
            if (win->focusEls[i].id == host) {
                WindowSetFocusId(win, host);
                return;
            }
        }
    }
    // No container of its own: the first thing in it that takes Tab, so the
    // trap still has focus to keep.
    FocusTrapEnter(win, trap);
}

} // namespace gpui
