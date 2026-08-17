#include "ui/Tooltip.h"
#include "ui/Primitive.h"

El* Tooltip::New(Arena* a, Str id) {
    return UiRoot(a, id, 0);
}
