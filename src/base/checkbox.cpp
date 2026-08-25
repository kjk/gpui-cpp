#include "base/checkbox.h"

namespace gpui {

CheckboxState CheckboxActivated(CheckboxState state) {
    return state == CheckboxState::Checked ? CheckboxState::Unchecked
                                           : CheckboxState::Checked;
}

El* Checkbox::New(Ctx* cx, Str id, CheckboxState state, bool disabled,
                  Listener onChange) {
    Arena* a = cx->a;
    // `div().id(id)` is unconditional in Rust; `track_focus` and `on_click`
    // both hang off `when(!disabled)`. The id is the fold of the name down
    // from the root, so a checkbox named among its siblings is still its own.
    El* e = Div(a)->PathClick(id);
    if (disabled) {
        return e;
    }
    e->PathId(id);
    if (onChange.IsValid()) {
        e->OnClick(ListenerFill(onChange, (intptr_t)CheckboxActivated(state)));
    }
    return e;
}

El* CheckboxIndicator::New(Ctx* cx) {
    Arena* a = cx->a;
    return Div(a);
}
} // namespace gpui
