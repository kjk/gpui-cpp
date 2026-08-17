#include "ui/Resizable.h"
#include "ui/Primitive.h"

El* Resizable::New(Arena* a, Str id) {
    return UiRoot(a, id, 0);
}
El* ResizablePanel::New(Arena* a) {
    return Div(a);
}
