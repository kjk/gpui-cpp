#include "ui/Button.h"
#include "ui/Primitive.h"

namespace gpui {

El* Button::New(Ctx* cx, Str id, int clickId) {
    Arena* a = cx->a;
    return UiRoot(a, id, clickId);
}
} // namespace gpui
