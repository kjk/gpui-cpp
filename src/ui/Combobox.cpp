#include "ui/Combobox.h"
#include "ui/Primitive.h"

namespace gpui {

El* Combobox::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return UiRoot(a, id, 0);
}
} // namespace gpui
