#include "ui/Link.h"
#include "ui/Primitive.h"

namespace gpui {

El* Link::New(Ctx* cx, Str id, int clickId) {
    Arena* a = cx->a;
    return UiRoot(a, id, clickId);
}
} // namespace gpui
