#include "ui/Toggle.h"
#include "ui/Primitive.h"

namespace gpui {

El* Toggle::New(Arena* a, Str id, int clickId) {
    return UiRoot(a, id, clickId);
}

El* ToggleGroup::New(Arena* a, Str id) {
    return UiRoot(a, id, 0);
}
} // namespace gpui
