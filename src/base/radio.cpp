#include "base/radio.h"

namespace gpui {

El* Radio::New(Ctx* cx, Str id, bool checked, bool disabled,
               Listener onChange) {
    Arena* a = cx->a;
    El* e = Div(a)
                ->PathClick(id)
                ->Role(AccessibilityRole::RadioButton)
                ->AriaToggled(checked ? AccessibilityToggled::True
                                      : AccessibilityToggled::False)
                ->AriaSelected(checked)
                ->AriaDisabled(disabled);
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
