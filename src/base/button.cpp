#include "base/button.h"

namespace gpui {

// The two semantic states, in resolve_style's order.
static void ApplyStyles(El* e, const ButtonStyles* styles, bool selected,
                        bool disabled) {
    if (!styles) {
        return;
    }
    const StateStyle* active[2] = {selected ? &styles->selected : nullptr,
                                   disabled ? &styles->disabled : nullptr};
    ElRefine(e, StateStyleResolve(StateStyle{}, active, 2));
}

El* Button::New(Ctx* cx, Str id, bool disabled, Listener onClick,
                bool focusable, const ButtonStyles* styles, bool selected) {
    Arena* a = cx->a;
    El* e = Div(a)->Id(id)->Click(HashClickId(id));
    ApplyStyles(e, styles, selected, disabled);
    if (disabled) {
        return e;
    }
    if (focusable) {
        e->FocusId(HashClickId(id));
    }
    if (onClick.IsValid()) {
        e->OnClick(onClick);
    }
    return e;
}
} // namespace gpui
