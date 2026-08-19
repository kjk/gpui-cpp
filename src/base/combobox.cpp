#include "base/combobox.h"
#include "base/element_ext.h"

namespace gpui {

El* Combobox::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return UiRoot(a, id, 0);
}
} // namespace gpui
