#include "component/DatePicker.h"

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
DatePicker* DatePicker::Placeholder(Str s) {
    placeholder = s;
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
    const Theme& th = cx->theme();
    bool hasDate = day > 0;
    Str title = hasDate
                    ? StrDup(a, fmt("%s %d, %d", Str(mon[month]), day, year))
                    : (placeholder.s ? placeholder : StrL("Select date"));
    // The trigger is input-shaped: the date (or placeholder) with a calendar
    // icon at the right edge.
    El* trigger = Div(a)
                      ->FlexRow()
                      ->W(kFill)
                      ->H(32)
                      ->PadX(10)
                      ->Gap(4)
                      ->ItemsCenter()
                      ->JustifyBetween()
                      ->Radius(th.radius)
                      ->Bg(th.inputBg)
                      ->Border(1, th.inputBorder);
    trigger->Child(
        TextEl(a, title)->Font(14)->Fg(hasDate ? th.foreground : th.mutedFg));
    trigger->Child(IconEl(a, IconName::Calendar, 12)->Fg(th.mutedFg));
    BindClick(trigger, StrL("date"), onToggle);
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
        ->W(kFill)
        ->Child(
            Popup::New(cx, StrL("date-pop"), trigger)->Content(cal)->IntoEl());
}

} // namespace component
} // namespace gpui
