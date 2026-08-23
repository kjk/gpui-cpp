#include "base/sheet.h"
#include "base/focus_trap.h"

namespace gpui {

SheetOverlayPress SheetOverlayPressAction(bool overlayInteractive,
                                          bool overlayClosable,
                                          MouseButton button, float pressY,
                                          bool hasDismissBefore,
                                          float dismissBeforeY) {
    if (!overlayInteractive) {
        return SheetOverlayPress::Ignore;
    }
    if (hasDismissBefore && pressY < dismissBeforeY) {
        return SheetOverlayPress::Ignore;
    }
    if (overlayClosable && button == MouseButton::Left) {
        return SheetOverlayPress::Close;
    }
    return SheetOverlayPress::Swallow;
}

bool SheetClosesOnKey(int key) {
    return key == KeyEscape;
}

Sheet* Sheet::New(Ctx* cx) {
    Arena* a = cx->a;
    Sheet* s = ArenaNew<Sheet>(a);
    s->cx = cx;
    s->trap = StrL("sheet");
    s->root = Div(a)->Fixed()->Top(0)->Left(0)->W(kFill)->H(kFill);
    return s;
}

Sheet* Sheet::Overlay(El* overlay) {
    if (overlay) {
        root->Child(overlay);
    }
    return this;
}

Sheet* Sheet::Trap(Str name) {
    trap = name;
    return this;
}

Sheet* Sheet::Surface(El* surface) {
    if (surface) {
        // focus_handle(self.focus_handle): the surface itself is where the
        // focus goes when the sheet opens — not a tab stop, so Tab still
        // visits the controls in it rather than the box around them, and no
        // ring lands on whichever one happens to be first.
        int id = FocusTrapId(trap);
        surface->TrapId(id)->FocusId(id)->TabStop(false)->FocusRing(false);
        FocusTrapArm(cx->win, id, id);
        root->Child(surface);
    }
    return this;
}

El* Sheet::IntoEl() {
    return root;
}
} // namespace gpui
