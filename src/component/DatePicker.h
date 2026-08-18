/* Themed date picker — crates/ui/src/time/date_picker.rs */

#include "component/Calendar.h"

namespace gpui {

namespace component {

struct DatePicker {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    int year = 2026;
    int month = 1;
    int day = 1; // 0: no date picked, so the placeholder shows
    Str placeholder = {};
    bool open = false;
    Listener onToggle;
    Listener onDay;

    static DatePicker* New(Ctx* cx);
    DatePicker* Year(int y);
    DatePicker* Month(int m);
    DatePicker* Day(int d);
    DatePicker* Placeholder(Str s);
    DatePicker* Open(bool v);
    DatePicker* OnToggle(Listener fn);
    DatePicker* OnDay(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
