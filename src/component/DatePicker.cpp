#include "component/DatePicker.h"
#include "component/Button.h"

namespace gpui {

namespace component {

DatePicker* DatePicker::New(Ctx* cx) {
    Arena* a = cx->a;
    DatePicker* d = ArenaNew<DatePicker>(a);
    d->a = a;
    d->cx = cx;
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
DatePicker* DatePicker::OnToggle(Listener fn) {
    onToggle = fn;
    return this;
}
DatePicker* DatePicker::OnDay(Listener fn) {
    onDay = fn;
    return this;
}

El* DatePicker::IntoEl() {
    static const char* mon[] = {"",    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    El* trigger =
        Button::New(cx, StrL("date"))
            ->Label(StrDup(a, fmt("%s %d, %d", Str(mon[month]), day, year)))
            ->OnClick(onToggle)
            ->IntoEl();
    El* cal = nullptr;
    if (open) {
        cal = Calendar::New(cx)
                  ->Year(year)
                  ->Month(month)
                  ->Day(day)
                  ->OnDay(onDay)
                  ->IntoEl();
    }
    return gpui::DatePicker::New(cx, StrL("date-picker"))
        ->Child(
            Popup::New(cx, StrL("date-pop"), trigger)->Content(cal)->IntoEl());
}

} // namespace component
} // namespace gpui
