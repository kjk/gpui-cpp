#include "Showcase.h"
#include "ui/Button.h"
#include "ui/DatePicker.h"
#include "ui/Popup.h"

enum {
    ClickDate = 340
};

El* ShowcaseDatePicker(ShowcaseApp* app, Arena* a) {
    El* trigger =
        Button::New(a, StrL("date-trigger"), ClickDate)
            ->W(250)
            ->H(28)
            ->PadX(12)
            ->ItemsCenter()
            ->JustifyBetween()
            ->Border(1, Rgb(0xa3, 0xa3, 0xa3))
            ->Bg(Rgb(0xff, 0xff, 0xff))
            ->HoverBg(Rgb(0xf5, 0xf5, 0xf5))
            ->Child(TextEl(a, StrL("Aug 12, 2026"))
                        ->Font(12)
                        ->Fg(Rgb(0x17, 0x17, 0x17)))
            ->Child(TextEl(a, StrL("⌄"))->Font(12)->Fg(Rgb(0x17, 0x17, 0x17)));
    El* cal = app->dateOpen ? ShowcaseCalendarGrid(app, a) : nullptr;
    return DatePicker::New(a, StrL("example-date-picker"))
        ->W(250)
        ->Child(Popup::New(a, StrL("date-picker-popup"), trigger)
                    ->Content(cal)
                    ->IntoEl());
}

void ShowcaseDatePickerClick(ShowcaseApp* app, int id) {
    if (id == ClickDate) {
        app->dateOpen = !app->dateOpen;
        return;
    }
    ShowcaseCalendarClick(app, id);
}

SHOWCASE_PAGE(CompDatePicker, ShowcaseDatePicker, ShowcaseDatePickerClick);
