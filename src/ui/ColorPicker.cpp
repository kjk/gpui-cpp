#include "ui/ColorPicker.h"
#include "ui/Primitive.h"

namespace gpui {

El* ColorPicker::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return UiRoot(a, id, 0);
}
El* ColorSwatch::New(Ctx* cx, Str id, int clickId) {
    Arena* a = cx->a;
    return UiRoot(a, id, clickId);
}
} // namespace gpui
