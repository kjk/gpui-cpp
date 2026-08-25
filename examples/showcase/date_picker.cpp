#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

static void ToggleDate(ShowcaseApp* app, Ctx* cx, const ClickEvent*) {
    app->dateOpen = !app->dateOpen;
    Notify(cx);
}

El* ShowcaseDatePicker(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    El* trigger =
        ScButton(cx, StrL("date-trigger"))
            ->OnClick(Listen(cx, &ToggleDate))
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
    El* cal = app->dateOpen ? ShowcaseCalendarGrid(app, cx) : nullptr;
    return DatePicker::New(cx, StrL("example-date-picker"))
        ->W(250)
        ->Child(Popup::New(cx, StrL("date-picker-popup"), trigger)
                    ->Content(cal)
                    ->IntoEl());
}

SHOWCASE_PAGE(CompDatePicker, ShowcaseDatePicker);
