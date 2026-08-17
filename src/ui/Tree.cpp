#include "ui/Tree.h"
#include "ui/Primitive.h"

namespace gpui {

El* Tree::New(Arena* a) {
    return UiRoot(a, StrL("example-tree"), 0);
}
El* TreeItem::New(Arena* a, int clickId) {
    return UiRoot(a, StrL("tree-item"), clickId);
}
} // namespace gpui
