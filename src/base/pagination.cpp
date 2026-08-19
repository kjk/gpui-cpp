#include "base/pagination.h"
#include "base/element_ext.h"

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
