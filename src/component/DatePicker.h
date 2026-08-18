/* Themed date picker — crates/ui/src/time/date_picker.rs */

#include "component/Calendar.h"

namespace gpui {

namespace component {

// The two formats the story uses: %Y/%m/%d (the default) and %Y-%m-%d.
enum class DateFormat : uint8_t {
    Slash,
    Dash
};

struct DatePicker {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    int year = 2026;
    int month = 1;
    int day = 1; // 0: no date picked, so the placeholder shows
    // The end of a range; year2 == 0 means a single date.
    int year2 = 0;
    int month2 = 0;
    int day2 = 0;
    Str placeholder = {};
    DateFormat format = DateFormat::Slash;
    float width = kFill;
    // cleanable swaps the calendar icon for a clear button once a date is set.
    bool cleanable = false;
    bool appearance = true;
    bool open = false;
    Listener onToggle;
    Listener onDay;
    Listener onClear;

    static DatePicker* New(Ctx* cx);
    DatePicker* Year(int y);
    DatePicker* Month(int m);
    DatePicker* Day(int d);
    DatePicker* RangeEnd(int y, int m, int d);
    DatePicker* Placeholder(Str s);
    DatePicker* Format(DateFormat f);
    DatePicker* W(float v);
    DatePicker* Cleanable(bool v = true);
    DatePicker* Appearance(bool v);
    DatePicker* Open(bool v);
    DatePicker* OnToggle(Listener fn);
    DatePicker* OnDay(Listener fn);
    DatePicker* OnClear(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
