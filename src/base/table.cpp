#include "base/table.h"

namespace gpui {

// Every part is a `Stateful<Div>` upstream — `div().id(id)` — so every one of
// them is a hit target, and the name it is given only has to be unique among
// its siblings. That is what puts the root on the chain a press walks, so a
// press on a row is a press on the table and the table is what takes the
// focus; and it is what a cell inside a clickable row can afford to be now
// that a click bubbles out of the rect it landed on.

El* Table::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return Div(a)->PathClick(id);
}
El* TableHeader::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return Div(a)->PathClick(id);
}
El* TableBody::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return Div(a)->PathClick(id);
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
    return Div(a)->PathClick(id);
}
El* TableCaption::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return Div(a)->PathClick(id);
}
} // namespace gpui
