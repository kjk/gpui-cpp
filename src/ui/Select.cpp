#include "ui/Select.h"
#include "ui/Primitive.h"

namespace gpui {

El* Select::New(Arena* a, Str id) {
    return UiRoot(a, id, 0);
}
} // namespace gpui
