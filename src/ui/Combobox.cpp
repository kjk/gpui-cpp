#include "ui/Combobox.h"
#include "ui/Primitive.h"

El* Combobox::New(Arena* a, Str id) {
    return UiRoot(a, id, 0);
}
