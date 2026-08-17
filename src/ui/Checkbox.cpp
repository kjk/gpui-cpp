#include "ui/Checkbox.h"
#include "ui/Primitive.h"

El* Checkbox::New(Arena* a, Str id, int clickId) {
    return UiRoot(a, id, clickId);
}

El* CheckboxIndicator::New(Arena* a) {
    return Div(a);
}
