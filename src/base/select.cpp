#include "base/select.h"

namespace gpui {

SelectAction SelectActionForKey(int key, bool open, bool disabled) {
    // Every one of Rust's handlers starts by propagating when disabled, so a
    // disabled select answers to none of them.
    if (disabled) {
        return SelectAction::None;
    }
    switch (key) {
        case KeyUp:
        case KeyDown:
            // Rust opens a closed select and then focuses the content either
            // way, so an open one has nothing left for this root to do — the
            // options themselves take the arrow from there.
            return open ? SelectAction::None : SelectAction::Open;
        case KeyReturn:
            return open ? SelectAction::Confirm : SelectAction::Open;
        case KeyEscape:
            // Escape on a closed select is not the select's; Rust propagates
            // it so whatever encloses the select can use it.
            return open ? SelectAction::Dismiss : SelectAction::None;
        default:
            return SelectAction::None;
    }
}

El* Select::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return Div(a)->Id(id);
}
} // namespace gpui
