#include "ui/Tooltip.h"
#include "ui/Primitive.h"

namespace gpui {

El* Tooltip::New(Arena* a, Str id) {
    return UiRoot(a, id, 0);
}
} // namespace gpui
