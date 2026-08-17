#include "ui/VirtualList.h"
#include "ui/Primitive.h"

namespace gpui {

El* VirtualList::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return UiRoot(a, id, 0);
}
} // namespace gpui
