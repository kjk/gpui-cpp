#include "ui/Calendar.h"
#include "ui/Primitive.h"

namespace gpui {

El* Calendar::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return UiRoot(a, id, 0);
}
El* CalendarItem::New(Ctx* cx, int clickId) {
    Arena* a = cx->a;
    return UiRoot(a, StrL("calendar-item"), clickId);
}
} // namespace gpui
