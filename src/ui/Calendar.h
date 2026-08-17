/* Unstyled calendar — crates/base/src/calendar.rs */

#include "gpui/Gpui.h"

namespace gpui {

struct Calendar {
    static El* New(Arena* a, Str id);
};
struct CalendarItem {
    static El* New(Arena* a, int clickId = 0);
};
} // namespace gpui
