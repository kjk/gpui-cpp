#include "ui/Calendar.h"
#include "ui/Primitive.h"

El* Calendar::New(Arena* a, Str id) {
    return UiRoot(a, id, 0);
}
El* CalendarItem::New(Arena* a, int clickId) {
    return UiRoot(a, StrL("calendar-item"), clickId);
}
