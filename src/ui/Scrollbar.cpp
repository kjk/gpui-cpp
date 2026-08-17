#include "ui/Scrollbar.h"
#include "ui/Primitive.h"

El* Scrollbar::New(Arena* a) {
    return UiRoot(a, StrL("example-scroll-region"), 0);
}
