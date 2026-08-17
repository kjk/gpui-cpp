#include "ui/Link.h"
#include "ui/Primitive.h"

namespace gpui {

El* Link::New(Arena* a, Str id, int clickId) {
    return UiRoot(a, id, clickId);
}
} // namespace gpui
