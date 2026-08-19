#include "base/radio.h"

namespace gpui {

El* Radio::New(Ctx* cx, Str id, bool checked, bool disabled,
               Listener onChange) {
    Arena* a = cx->a;
    int clickId = HashClickId(id);
    El* e = Div(a)->Id(id)->Click(clickId);
    if (disabled) {
        return e;
    }
    e->FocusId(clickId);
    if (!checked && onChange.IsValid()) {
        e->OnClick(ListenerFill(onChange, true));
    }
    return e;
}
} // namespace gpui
