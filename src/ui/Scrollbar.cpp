#include "ui/Scrollbar.h"
#include "ui/Primitive.h"

namespace gpui {

El* Scrollbar::New(Arena* a) {
    return UiRoot(a, StrL("example-scroll-region"), 0);
}
} // namespace gpui
