/* Themed calendar — crates/ui/src/time/calendar.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

struct Calendar {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    int year = 2026;
    int month = 1;
    int day = 1;
    Listener onDay; // day of month
    Listener onPrev;
    Listener onNext;

    static Calendar* New(Ctx* cx);
    Calendar* Year(int y);
    Calendar* Month(int m);
    Calendar* Day(int d);
    Calendar* OnDay(Listener fn);
    Calendar* OnPrev(Listener fn);
    Calendar* OnNext(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
