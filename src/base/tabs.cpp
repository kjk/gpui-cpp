#include "base/tabs.h"

namespace gpui {

El* Tabs::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return Div(a)->Id(id);
}

El* Tab::New(Ctx* cx, Str id, bool disabled, Listener onClick) {
    Arena* a = cx->a;
    El* e = Div(a)->Id(id)->Click(HashClickId(id));
    if (!disabled && onClick.IsValid()) {
        e->OnClick(onClick);
    }
    return e;
}
} // namespace gpui
