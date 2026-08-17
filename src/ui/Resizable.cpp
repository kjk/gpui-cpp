#include "ui/Resizable.h"
#include "ui/Primitive.h"

namespace gpui {

El* Resizable::New(Arena* a, Str id) {
    return UiRoot(a, id, 0);
}
El* ResizablePanel::New(Arena* a) {
    return Div(a);
}
} // namespace gpui
