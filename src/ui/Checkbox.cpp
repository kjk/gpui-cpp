#include "ui/Checkbox.h"
#include "ui/Primitive.h"

namespace gpui {

El* Checkbox::New(Ctx* cx, Str id, int clickId) {
    Arena* a = cx->a;
    return UiRoot(a, id, clickId);
}

El* CheckboxIndicator::New(Ctx* cx) {
    Arena* a = cx->a;
    return Div(a);
}
} // namespace gpui
