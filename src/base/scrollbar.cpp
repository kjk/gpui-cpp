#include "base/scrollbar.h"
#include "base/element_ext.h"

namespace gpui {

El* Scrollbar::New(Ctx* cx) {
    Arena* a = cx->a;
    return UiRoot(a, StrL("example-scroll-region"), 0);
}
} // namespace gpui
