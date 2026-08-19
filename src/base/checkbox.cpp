#include "base/checkbox.h"

namespace gpui {

CheckboxState CheckboxActivated(CheckboxState state) {
    return state == CheckboxState::Checked ? CheckboxState::Unchecked
                                           : CheckboxState::Checked;
}

El* Checkbox::New(Ctx* cx, Str id, CheckboxState state, bool disabled,
                  Listener onChange) {
    Arena* a = cx->a;
    int clickId = HashClickId(id);
    // `div().id(id)` is unconditional in Rust; `track_focus` and `on_click`
    // both hang off `when(!disabled)`.
    El* e = Div(a)->Id(id)->Click(clickId);
    if (disabled) {
        return e;
    }
    e->FocusId(clickId);
    if (onChange.IsValid()) {
        e->OnClick(ListenerArg(onChange, (intptr_t)CheckboxActivated(state)));
    }
    return e;
}

El* CheckboxIndicator::New(Ctx* cx) {
    Arena* a = cx->a;
    return Div(a);
}
} // namespace gpui
