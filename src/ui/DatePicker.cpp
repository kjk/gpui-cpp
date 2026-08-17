#include "ui/DatePicker.h"
#include "ui/Primitive.h"

namespace gpui {

El* DatePicker::New(Arena* a, Str id) {
    return UiRoot(a, id, 0);
}
} // namespace gpui
