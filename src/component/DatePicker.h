/* Themed date picker — crates/ui/src/time/date_picker.rs */

#pragma once

#include "component/Calendar.h"

namespace component {

struct DatePicker {
    Arena* a = nullptr;
    int year = 2026;
    int month = 1;
    int day = 1;
    bool open = false;
    Func0 onToggle;
    Func1<int> onDay;

    static DatePicker* New(Arena* a);
    DatePicker* Year(int y);
    DatePicker* Month(int m);
    DatePicker* Day(int d);
    DatePicker* Open(bool v);
    DatePicker* OnToggle(Func0 fn);
    DatePicker* OnDay(Func1<int> fn);
    El* IntoEl();
};

} // namespace component
