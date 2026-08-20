#include "base/dialog.h"
#include "base/element_ext.h"
#include "base/focus_trap.h"

namespace gpui {

DialogAction DialogActionForKey(int key, bool keyboard) {
    // Rust hangs the key context off `keyboard`, so the bindings do not exist
    // at all for a dialog that turned it off.
    if (!keyboard) {
        return DialogAction::None;
    }
    if (key == KeyEscape) {
        return DialogAction::Cancel;
    }
    if (key == KeyReturn) {
        return DialogAction::Confirm;
    }
    return DialogAction::None;
}

bool DialogBackdropCloses(bool overlayClosable, bool topmost,
                          MouseButton button, float pressY,
                          float dismissBelowY) {
    // Above the reserved band the press is not the backdrop's — that is where
    // a title bar a dialog was opened over still is.
    if (pressY < dismissBelowY) {
        return false;
    }
    // Rust computes `overlay_closable && topmost` once, so a dialog under
    // another one never answers a backdrop press.
    return button == MouseButton::Left && overlayClosable && topmost;
}

El* DialogTrigger::New(Ctx* cx, Listener onOpen) {
    Arena* a = cx->a;
    El* e = Div(a);
    if (onOpen.IsValid()) {
        e->OnMouseDown(onOpen);
    }
    return e;
}

El* DialogBackdrop::New(Ctx* cx) {
    Arena* a = cx->a;
    return UiRoot(a, StrL("dialog-backdrop"), 0);
}
El* DialogPopup::New(Ctx* cx) {
    Arena* a = cx->a;
    return UiRoot(a, StrL("dialog-popup"), 0);
}
El* DialogTitle::New(Ctx* cx) {
    Arena* a = cx->a;
    return UiRoot(a, StrL("dialog-title"), 0);
}
El* DialogDescription::New(Ctx* cx) {
    Arena* a = cx->a;
    return UiRoot(a, StrL("dialog-description"), 0);
}
El* DialogClose::New(Ctx* cx, int clickId) {
    Arena* a = cx->a;
    return UiRoot(a, StrL("dialog-close"), clickId);
}

Dialog* Dialog::New(Ctx* cx) {
    Arena* a = cx->a;
    Dialog* d = ArenaNew<Dialog>(a);
    d->cx = cx;
    d->trap = StrL("dialog");
    d->root = Div(a)->Fixed()->Top(0)->Left(0)->W(kFill)->H(kFill);
    return d;
}

Dialog* Dialog::Trap(Str name) {
    trap = name;
    return this;
}

Dialog* Dialog::Backdrop(El* backdrop) {
    if (backdrop) {
        root->Child(backdrop);
    }
    return this;
}

Dialog* Dialog::Popup(El* popup) {
    if (popup) {
        // The popup is the trap container, not the backdrop: a Tab inside a
        // dialog reaches its own controls and nothing behind it.
        int id = FocusTrapId(trap);
        popup->TrapId(id);
        FocusTrapArm(cx->win, id);
        root->Child(popup);
    }
    return this;
}

El* Dialog::IntoEl() {
    return root;
}
} // namespace gpui
