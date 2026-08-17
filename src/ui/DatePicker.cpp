#include "ui/DatePicker.h"
#include "ui/Primitive.h"

El* DatePicker::New(Arena* a, Str id) {
    return UiRoot(a, id, 0);
}
