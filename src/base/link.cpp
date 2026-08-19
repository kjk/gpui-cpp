#include "base/link.h"
#include "base/element_ext.h"

namespace gpui {

El* Link::New(Ctx* cx, Str id, int clickId) {
    Arena* a = cx->a;
    return UiRoot(a, id, clickId);
}
} // namespace gpui
