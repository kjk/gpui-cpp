#include "base/tabs.h"
#include "base/element_ext.h"

namespace gpui {

El* Tabs::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return UiRoot(a, id, 0);
}
El* Tab::New(Ctx* cx, Str id, int clickId) {
    Arena* a = cx->a;
    return UiRoot(a, id, clickId);
}
} // namespace gpui
