#include "base/select.h"
#include "base/actions.h"
#include "gpui/keymap.h"

namespace gpui {

Str SelectContext() {
    return StrL("Select");
}

void SelectInitKeys() {
    static uint32_t bound = 0;
    if (bound == KeymapGeneration()) {
        return;
    }
    bound = KeymapGeneration();
    const char* ctx = "Select";
    KeyBinding bindings[] = {
        {"up", action::SelectUp(), ctx},
        {"down", action::SelectDown(), ctx},
        {"enter", action::Confirm(), ctx},
        // Confirm { secondary: true }, which has no payload to carry here.
        {"secondary-enter", action::ConfirmSecondary(), ctx},
        {"escape", action::Cancel(), ctx},
    };
    KeymapBind(bindings, (int)(sizeof(bindings) / sizeof(bindings[0])));
}

SelectAction SelectActionOf(uint32_t id, bool open, bool disabled) {
    // Every one of Rust's handlers starts by propagating when disabled, so a
    // disabled select answers to none of them.
    if (disabled) {
        return SelectAction::None;
    }
    if (id == action::SelectUp() || id == action::SelectDown()) {
        // Rust opens a closed select and then focuses the content either
        // way, so an open one has nothing left for this root to do — the
        // options themselves take the arrow from there.
        return open ? SelectAction::None : SelectAction::Open;
    }
    if (id == action::Confirm() || id == action::ConfirmSecondary()) {
        return open ? SelectAction::Confirm : SelectAction::Open;
    }
    if (id == action::Cancel()) {
        // Escape on a closed select is not the select's; Rust propagates
        // it so whatever encloses the select can use it.
        return open ? SelectAction::Dismiss : SelectAction::None;
    }
    return SelectAction::None;
}

El* Select::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return Div(a)->Id(id);
}
} // namespace gpui
