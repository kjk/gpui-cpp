#include "ui/VirtualList.h"
#include "ui/Primitive.h"

El* VirtualList::New(Arena* a, Str id) {
    return UiRoot(a, id, 0);
}
