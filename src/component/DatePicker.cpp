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
DatePicker* DatePicker::RangeEnd(int y, int m, int d) {
    year2 = y;
    month2 = m;
    day2 = d;
    return this;
}
DatePicker* DatePicker::Format(DateFormat f) {
    format = f;
    return this;
}
DatePicker* DatePicker::W(float v) {
    width = v;
    return this;
}
DatePicker* DatePicker::Cleanable(bool v) {
    cleanable = v;
    return this;
}
DatePicker* DatePicker::Appearance(bool v) {
    appearance = v;
    return this;
}
DatePicker* DatePicker::OnClear(Listener fn) {
    onClear = fn;
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

static Str FormatDate(Arena* a, DateFormat f, int y, int m, int d) {
    const char* sep = f == DateFormat::Dash ? "-" : "/";
    return StrDup(a, fmt("%d%s%02d%s%02d", y, Str(sep), m, Str(sep), d));
}

El* DatePicker::IntoEl() {
    const Theme& th = cx->theme();
    bool hasDate = day > 0;
    Str title;
    if (!hasDate) {
        title = placeholder.s ? placeholder : StrL("Select date");
    } else if (year2 > 0) {
        title =
            StrDup(a, fmt("%s - %s", FormatDate(a, format, year, month, day),
                          FormatDate(a, format, year2, month2, day2)));
    } else {
        title = FormatDate(a, format, year, month, day);
    }
    // The trigger is input-shaped: the date (or placeholder) with a calendar
    // icon, or the clear button when there is something to clear.
    El* trigger = Div(a)
                      ->FlexRow()
                      ->W(width)
                      ->H(32)
                      ->PadX(10)
                      ->Gap(4)
                      ->ItemsCenter()
                      ->JustifyBetween();
    if (appearance) {
        trigger->Radius(th.radius)->Bg(th.inputBg)->Border(1, th.inputBorder);
    }
    trigger->Child(
        TextEl(a, title)->Font(14)->Fg(hasDate ? th.foreground : th.mutedFg));
    if (cleanable && hasDate) {
        trigger->Child(Button::New(cx, StrL("date-clean"))
                           ->Text()
                           ->WithSize(UiSize::XSmall)
                           ->Icon(IconName::X)
                           ->OnClick(onClear)
                           ->IntoEl());
    } else {
        trigger->Child(IconEl(a, IconName::Calendar, 12)->Fg(th.mutedFg));
    }
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
        ->W(width)
        ->Child(
            Popup::New(cx, StrL("date-pop"), trigger)->Content(cal)->IntoEl());
}

} // namespace component
} // namespace gpui
