#include "base/button.h"

namespace gpui {

El* Button::New(Ctx* cx, Str id, bool disabled, Listener onClick,
                bool focusable) {
    Arena* a = cx->a;
    El* e = Div(a)->Id(id)->Click(HashClickId(id));
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
