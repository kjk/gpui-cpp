/* Themed calendar — crates/ui/src/time/calendar.rs */

#pragma once

#include "component/Common.h"

namespace component {

struct Calendar {
    Arena* a = nullptr;
    int year = 2026;
    int month = 1;
    int day = 1;
    Func1<int> onDay; // day of month
    Func0 onPrev;
    Func0 onNext;

    static Calendar* New(Arena* a);
    Calendar* Year(int y);
    Calendar* Month(int m);
    Calendar* Day(int d);
    Calendar* OnDay(Func1<int> fn);
    Calendar* OnPrev(Func0 fn);
    Calendar* OnNext(Func0 fn);
    El* IntoEl();
};

} // namespace component
