#include "ui/Pagination.h"
#include "ui/Primitive.h"

namespace gpui {

El* Pagination::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return UiRoot(a, id, 0);
}
El* PaginationItem::New(Ctx* cx, Str id, int clickId) {
    Arena* a = cx->a;
    return UiRoot(a, id, clickId);
}
} // namespace gpui
