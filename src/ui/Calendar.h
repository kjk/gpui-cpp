/* Unstyled calendar — crates/base/src/calendar.rs */

#include "gpui/Gpui.h"

namespace gpui {

struct Calendar {
    static El* New(Ctx* cx, Str id);
};
struct CalendarItem {
    static El* New(Ctx* cx, int clickId = 0);
};
} // namespace gpui
