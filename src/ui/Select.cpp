#include "ui/Select.h"
#include "ui/Primitive.h"

El* Select::New(Arena* a, Str id) {
    return UiRoot(a, id, 0);
}
