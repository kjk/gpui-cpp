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
    // `div().id(self.id)`: a button's name only has to be unique among its
    // siblings, since GPUI scopes it by the stack of ids above it. That is why
    // upstream writes `Button::new("prev")` inside a pagination rather than
    // `Button::new(format!("{base}-prev"))` — the fold does that part.
    El* e = Div(a)
                ->PathClick(id)
                ->SuppressTextSelection()
                ->Role(AccessibilityRole::Button)
                ->AriaDisabled(disabled);
    ApplyStyles(e, styles, selected, disabled);
    if (disabled) {
        return e;
    }
    if (focusable) {
        // `.track_focus(&handle.tab_index(..).tab_stop(..))`, still standing
        // in for a handle with the same fold the hit id comes from.
        e->PathId(id);
    }
    if (onClick.IsValid()) {
        e->OnClick(onClick);
    }
    return e;
}
} // namespace gpui
