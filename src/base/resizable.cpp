#include "base/resizable.h"
#include "base/element_ext.h"

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
