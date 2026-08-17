#include "ui/Tabs.h"
#include "ui/Primitive.h"

namespace gpui {

El* Tabs::New(Arena* a, Str id) {
    return UiRoot(a, id, 0);
}
El* Tab::New(Arena* a, Str id, int clickId) {
    return UiRoot(a, id, clickId);
}
} // namespace gpui
