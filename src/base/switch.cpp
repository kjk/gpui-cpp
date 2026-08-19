#include "base/switch.h"

namespace gpui {

El* Switch::New(Ctx* cx, Str id, bool checked, bool disabled,
                Listener onChange) {
    Arena* a = cx->a;
    int clickId = HashClickId(id);
    El* e = Div(a)->Id(id)->Click(clickId);
    if (disabled) {
        return e;
    }
    e->FocusId(clickId);
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
