#include "base/toggle.h"

namespace gpui {

El* Toggle::New(Ctx* cx, Str id, bool pressed, bool disabled,
                Listener onChange) {
    Arena* a = cx->a;
    El* e = Div(a)->PathClick(id);
    if (disabled) {
        return e;
    }
    e->PathId(id);
    if (onChange.IsValid()) {
        e->OnClick(ListenerFill(onChange, !pressed));
    }
    return e;
}
} // namespace gpui
