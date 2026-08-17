#include "ui/Resizable.h"
#include "ui/Primitive.h"

namespace gpui {

El* Resizable::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return UiRoot(a, id, 0);
}
El* ResizablePanel::New(Ctx* cx) {
    Arena* a = cx->a;
    return Div(a);
}
} // namespace gpui
