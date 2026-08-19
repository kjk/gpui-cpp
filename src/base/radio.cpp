#include "base/radio.h"
#include "base/element_ext.h"

namespace gpui {

El* Radio::New(Ctx* cx, Str id, int clickId) {
    Arena* a = cx->a;
    return UiRoot(a, id, clickId);
}

El* RadioGroup::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return UiRoot(a, id, 0);
}
} // namespace gpui
