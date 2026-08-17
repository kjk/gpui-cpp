#include "ui/Checkbox.h"
#include "ui/Primitive.h"

namespace gpui {

El* Checkbox::New(Arena* a, Str id, int clickId) {
    return UiRoot(a, id, clickId);
}

El* CheckboxIndicator::New(Arena* a) {
    return Div(a);
}
} // namespace gpui
