#include "base/tabs.h"

namespace gpui {

El* Tabs::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return Div(a)->Id(id);
}

El* Tab::New(Ctx* cx, Str id, bool disabled, Listener onClick) {
    Arena* a = cx->a;
    // `div().id(ix)`: a tab is named by its place in the strip, which the bar
    // above it scopes.
    El* e = Div(a)->PathClick(id);
    if (!disabled && onClick.IsValid()) {
        e->OnClick(onClick);
    }
    return e;
}
} // namespace gpui
