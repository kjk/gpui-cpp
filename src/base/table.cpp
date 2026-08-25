#include "base/table.h"

namespace gpui {

El* Table::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return Div(a)->Id(id);
}
El* TableHeader::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return Div(a)->Id(id);
}
El* TableBody::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return Div(a)->Id(id);
}
El* TableRow::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return Div(a)->PathClick(id);
}
El* TableHead::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return Div(a)->PathClick(id);
}
El* TableCell::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return Div(a)->Id(id);
}
El* TableCaption::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return Div(a)->Id(id);
}
} // namespace gpui
