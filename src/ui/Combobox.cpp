#include "ui/Combobox.h"
#include "ui/Primitive.h"

namespace gpui {

El* Combobox::New(Arena* a, Str id) {
    return UiRoot(a, id, 0);
}
} // namespace gpui
