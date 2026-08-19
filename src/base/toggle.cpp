#include "base/toggle.h"
#include "base/element_ext.h"

namespace gpui {

El* Toggle::New(Ctx* cx, Str id, int clickId) {
    Arena* a = cx->a;
    return UiRoot(a, id, clickId);
}

El* ToggleGroup::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return UiRoot(a, id, 0);
}
} // namespace gpui
