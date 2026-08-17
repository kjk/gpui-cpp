/* Themed date picker — crates/ui/src/time/date_picker.rs */

#include "component/Calendar.h"

namespace gpui {

namespace component {

struct DatePicker {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    int year = 2026;
    int month = 1;
    int day = 1;
    bool open = false;
    Func0 onToggle;
    Func1<int> onDay;

    static DatePicker* New(Ctx* cx);
    DatePicker* Year(int y);
    DatePicker* Month(int m);
    DatePicker* Day(int d);
    DatePicker* Open(bool v);
    DatePicker* OnToggle(Func0 fn);
    DatePicker* OnDay(Func1<int> fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
