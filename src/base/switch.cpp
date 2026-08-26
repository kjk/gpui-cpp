#include "base/switch.h"

namespace gpui {

El* Switch::New(Ctx* cx, Str id, bool checked, bool disabled,
                Listener onChange) {
    Arena* a = cx->a;
    El* e = Div(a)
                ->PathClick(id)
                ->Role(AccessibilityRole::Switch)
                ->AriaToggled(checked ? AccessibilityToggled::True
                                      : AccessibilityToggled::False)
                ->AriaDisabled(disabled);
    if (disabled) {
        return e;
    }
    e->PathId(id);
    if (onChange.IsValid()) {
        e->OnClick(ListenerFill(onChange, !checked));
    }
    return e;
}

El* SwitchTrack::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return Div(a)->Id(id);
}

El* SwitchThumb::New(Ctx* cx) {
    Arena* a = cx->a;
    return Div(a);
}
} // namespace gpui
