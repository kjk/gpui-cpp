#include "base/radio.h"

namespace gpui {

RadioStyles& RadioStyles::Checked(const StateStyle& style) {
    StateStyleRefine(&checked, style);
    return *this;
}

RadioStyles& RadioStyles::Disabled(const StateStyle& style) {
    StateStyleRefine(&disabled, style);
    return *this;
}

El* Radio::New(Ctx* cx, Str id, bool checked, bool disabled, Listener onChange,
               const RadioStyles* styles, const StateStyle* instance) {
    Arena* a = cx->a;
    El* e = Div(a)
                ->PathClick(id)
                ->Role(AccessibilityRole::RadioButton)
                ->AriaToggled(checked ? AccessibilityToggled::True
                                      : AccessibilityToggled::False)
                ->AriaSelected(checked)
                ->AriaDisabled(disabled);
    if (styles || instance) {
        StateStyle base = instance ? *instance : StateStyle{};
        const StateStyle* states[2] = {
            checked && styles ? &styles->checked : nullptr,
            disabled && styles ? &styles->disabled : nullptr,
        };
        ElRefine(e, StateStyleResolve(base, states, 2));
    }
    if (disabled) {
        return e;
    }
    e->PathId(id);
    if (!checked && onChange.IsValid()) {
        e->OnClick(ListenerFill(onChange, true));
    }
    return e;
}
} // namespace gpui
