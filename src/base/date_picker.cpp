#include "base/date_picker.h"
#include "base/element_ext.h"

namespace gpui {

El* DatePicker::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return UiRoot(a, id, 0);
}
} // namespace gpui
