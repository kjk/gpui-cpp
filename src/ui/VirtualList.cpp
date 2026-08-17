#include "ui/VirtualList.h"
#include "ui/Primitive.h"

namespace gpui {

El* VirtualList::New(Arena* a, Str id) {
    return UiRoot(a, id, 0);
}
} // namespace gpui
