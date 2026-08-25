#include "base/table.h"

namespace gpui {

// Every part is a `Stateful<Div>` upstream — `div().id(id)` — so every one of
// them is a hit target. Here only the ones that need to be are: the root,
// because that is what puts it on the chain a press walks (a press on a row is
// *on* the table, which is how the table takes focus from it), and the row and
// the head, which take clicks. A click is delivered to the innermost hit rect
// alone rather than bubbling out of it, so a cell made hit-testable for the
// sake of the shape would swallow the click its row is listening for.

El* Table::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return Div(a)->PathClick(id);
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
