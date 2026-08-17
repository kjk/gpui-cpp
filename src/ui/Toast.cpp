#include "ui/Toast.h"
#include "ui/Primitive.h"

El* Toast::New(Arena* a, Str id) {
    return UiRoot(a, id, 0);
}
