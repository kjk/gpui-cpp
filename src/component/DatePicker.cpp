#include "component/DatePicker.h"
#include "component/Button.h"

namespace component {

DatePicker* DatePicker::New(Arena* a) {
    DatePicker* d = ::New<DatePicker>(a);
    d->a = a;
    return d;
}
DatePicker* DatePicker::Year(int y) {
    year = y;
    return this;
}
DatePicker* DatePicker::Month(int m) {
    month = m;
    return this;
}
DatePicker* DatePicker::Day(int d) {
    day = d;
    return this;
}
DatePicker* DatePicker::Open(bool v) {
    open = v;
    return this;
}
DatePicker* DatePicker::OnToggle(Func0 fn) {
    onToggle = fn;
    return this;
}
DatePicker* DatePicker::OnDay(Func1<int> fn) {
    onDay = fn;
    return this;
}

El* DatePicker::IntoEl() {
    static const char* mon[] = {"",    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    El* trigger =
        Button::New(a, StrL("date"))
            ->Label(StrDup(a, fmt("%s %d, %d", Str(mon[month]), day, year)))
            ->OnClick(onToggle)
            ->IntoEl();
    El* cal = nullptr;
    if (open) {
        cal = Calendar::New(a)
                  ->Year(year)
                  ->Month(month)
                  ->Day(day)
                  ->OnDay(onDay)
                  ->IntoEl();
    }
    return ::DatePicker::New(a, StrL("date-picker"))
        ->Child(
            Popup::New(a, StrL("date-pop"), trigger)->Content(cal)->IntoEl());
}

} // namespace component
