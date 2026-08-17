#include "ui/Toast.h"
#include "ui/Primitive.h"

namespace gpui {

El* Toast::New(Arena* a, Str id) {
    return UiRoot(a, id, 0);
}
} // namespace gpui
