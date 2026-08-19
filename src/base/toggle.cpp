#include "base/toggle.h"

namespace gpui {

El* Toggle::New(Ctx* cx, Str id, bool pressed, bool disabled,
                Listener onChange) {
    Arena* a = cx->a;
    int clickId = HashClickId(id);
    El* e = Div(a)->Id(id)->Click(clickId);
    if (disabled) {
        return e;
    }
    e->FocusId(clickId);
    if (onChange.IsValid()) {
        e->OnClick(ListenerFill(onChange, !pressed));
    }
    return e;
}
} // namespace gpui
