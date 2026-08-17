#include "ui/Radio.h"
#include "ui/Primitive.h"

El* Radio::New(Arena* a, Str id, int clickId) {
    return UiRoot(a, id, clickId);
}

El* RadioGroup::New(Arena* a, Str id) {
    return UiRoot(a, id, 0);
}
