#include "ui/Tree.h"
#include "ui/Primitive.h"

El* Tree::New(Arena* a) {
    return UiRoot(a, StrL("example-tree"), 0);
}
El* TreeItem::New(Arena* a, int clickId) {
    return UiRoot(a, StrL("tree-item"), clickId);
}
