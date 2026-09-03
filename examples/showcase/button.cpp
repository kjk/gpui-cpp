#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

static void SaveClicked(ShowcaseApp*, Ctx*, const ClickEvent*) {
    log(StrL("Save changes"));
}

static void CancelClicked(ShowcaseApp*, Ctx*, const ClickEvent*) {
    log(StrL("Cancel"));
}

// Button::styles(|s| s.selected(..).disabled(..)). The disabled half is what
// Rust's own test names — `disabled(|style| style.opacity(0.5))` — and the
// selected half is what a toggled button in a toolbar looks like. Both land at
// layout, so they win over the colours chained on below.
static const ButtonStyles kButtonStyles = [] {
    ButtonStyles s;
    s.selected.Bg(ExampleRgb(0x404040)).HoverBg(ExampleRgb(0x404040));
    s.disabled.Opacity(0.5f);
    return s;
}();

El* ShowcaseButton(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    (void)app;
    return Div(a)
        ->FlexRow()
        ->ItemsCenter()
        ->Gap(8)
        ->Child(ScButton(cx, StrL("primary-button"))
                    ->OnClick(Listen(cx, &SaveClicked))
                    ->PadX(12)
                    ->H(28)
                    ->ItemsCenter()
                    ->Font(12)
                    ->Border(1, ExampleRgb(0x171717))
                    ->Bg(ExampleRgb(0x171717))
                    ->HoverBg(ExampleRgb(0x404040))
                    ->Child(TextEl(a, StrL("Save changes"))
                                ->Font(12)
                                ->Fg(ExampleRgb(0xffffff))))
        ->Child(ScButton(cx, StrL("secondary-button"))
                    ->OnClick(Listen(cx, &CancelClicked))
                    ->PadX(12)
                    ->H(28)
                    ->ItemsCenter()
                    ->Font(12)
                    ->Border(1, ExampleRgb(0xd4d4d4))
                    ->Bg(ExampleRgb(0xffffff))
                    ->HoverBg(ExampleRgb(0xf5f5f5))
                    ->Child(TextEl(a, StrL("Cancel"))
                                ->Font(12)
                                ->Fg(ExampleRgb(0x171717))))
        // The same chain three times over; what tells the three apart is the
        // state the primitive resolves, not a second set of colours here.
        ->Child(
            ScButton(cx, StrL("selected-button"), false, &kButtonStyles, true)
                ->PadX(12)
                ->H(28)
                ->ItemsCenter()
                ->Font(12)
                ->Border(1, ExampleRgb(0x171717))
                ->Bg(ExampleRgb(0x171717))
                ->HoverBg(ExampleRgb(0x404040))
                ->Child(TextEl(a, StrL("Selected"))
                            ->Font(12)
                            ->Fg(ExampleRgb(0xffffff))))
        ->Child(ScButton(cx, StrL("disabled-button"), true, &kButtonStyles)
                    ->PadX(12)
                    ->H(28)
                    ->ItemsCenter()
                    ->Font(12)
                    ->Border(1, ExampleRgb(0x171717))
                    ->Bg(ExampleRgb(0x171717))
                    ->HoverBg(ExampleRgb(0x404040))
                    ->Child(TextEl(a, StrL("Disabled"))
                                ->Font(12)
                                ->Fg(ExampleRgb(0xffffff))));
}

SHOWCASE_PAGE(CompButton, ShowcaseButton);
