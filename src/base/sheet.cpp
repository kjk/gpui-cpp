#include "base/sheet.h"

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
    s->root = Div(a)->Fixed()->Top(0)->Left(0)->W(kFill)->H(kFill);
    return s;
}

Sheet* Sheet::Overlay(El* overlay) {
    if (overlay) {
        root->Child(overlay);
    }
    return this;
}

Sheet* Sheet::Surface(El* surface) {
    if (surface) {
        root->Child(surface);
    }
    return this;
}

El* Sheet::IntoEl() {
    return root;
}
} // namespace gpui
