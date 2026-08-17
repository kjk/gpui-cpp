#include "ui/Table.h"
#include "ui/Primitive.h"

namespace gpui {

El* Table::New(Arena* a, Str id) {
    return UiRoot(a, id, 0);
}
El* TableHeader::New(Arena* a, Str id) {
    return UiRoot(a, id, 0);
}
El* TableBody::New(Arena* a, Str id) {
    return UiRoot(a, id, 0);
}
El* TableRow::New(Arena* a, Str id) {
    return UiRoot(a, id, 0);
}
El* TableHead::New(Arena* a, Str id) {
    return UiRoot(a, id, 0);
}
El* TableCell::New(Arena* a, Str id) {
    return UiRoot(a, id, 0);
}
} // namespace gpui
