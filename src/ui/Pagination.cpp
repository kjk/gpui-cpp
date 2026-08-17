#include "ui/Pagination.h"
#include "ui/Primitive.h"

El* Pagination::New(Arena* a, Str id) {
    return UiRoot(a, id, 0);
}
El* PaginationItem::New(Arena* a, Str id, int clickId) {
    return UiRoot(a, id, clickId);
}
