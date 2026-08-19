#include "base/virtual_list.h"
#include "base/element_ext.h"

namespace gpui {

El* VirtualList::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return UiRoot(a, id, 0);
}
} // namespace gpui
