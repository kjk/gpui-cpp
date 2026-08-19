#include "base/tree.h"
#include "base/element_ext.h"

namespace gpui {

El* Tree::New(Ctx* cx) {
    Arena* a = cx->a;
    return UiRoot(a, StrL("example-tree"), 0);
}
El* TreeItem::New(Ctx* cx, int clickId) {
    Arena* a = cx->a;
    return UiRoot(a, StrL("tree-item"), clickId);
}
} // namespace gpui
