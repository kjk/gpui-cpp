#include "ui/Button.h"
#include "ui/Primitive.h"

namespace gpui {

El* Button::New(Arena* a, Str id, int clickId) {
    return UiRoot(a, id, clickId);
}
} // namespace gpui
