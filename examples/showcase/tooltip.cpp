#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

// on_hover on the trigger: the page is *told* when the pointer arrives and
// leaves, and keeps the answer. Rust's `self.tooltip_visible`.
static void TooltipHover(ShowcaseApp* app, Ctx* cx, const HoverEvent* ev) {
    if (app->tooltipVisible == ev->hovered) {
        return;
    }
    app->tooltipVisible = ev->hovered;
    Notify(cx);
}

El* ShowcaseTooltip(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    // Rust wraps the button in a hover target; the button itself has no hover
    // fill.
    El* btn = Button::New(cx, StrL("tooltip-anchor"))
                  ->H(28)
                  ->PadX(8)
                  ->ItemsCenter()
                  ->JustifyCenter()
                  ->Border(1, Rgb(0x17, 0x17, 0x17))
                  ->Bg(Rgb(0xff, 0xff, 0xff))
                  ->Child(TextEl(a, StrL("Command menu"))
                              ->Font(12)
                              ->Fg(Rgb(0x17, 0x17, 0x17)));
    El* trigger = Div(a)
                      ->PathClick(StrL("tooltip-trigger"))
                      ->OnHover(Listen(cx, &TooltipHover))
                      ->Child(btn);
    El* tip = nullptr;
    if (app->tooltipVisible) {
        tip = Tooltip::New(cx, StrL("example-tooltip"))
                  ->AnchorBelow(0)
                  ->Left(0)
                  ->PadX(8)
                  ->H(28)
                  ->ItemsCenter()
                  ->JustifyCenter()
                  ->Border(1, Rgb(0x17, 0x17, 0x17))
                  ->Bg(Rgb(0x17, 0x17, 0x17))
                  ->Child(TextEl(a, StrL("Open command menu · \xE2\x8C\x98K"))
                              ->Font(12)
                              ->Fg(Rgb(0xff, 0xff, 0xff)));
    }
    return Popup::New(cx, StrL("example-tooltip-popup"), trigger)
        ->Content(tip)
        ->IntoEl();
}

SHOWCASE_PAGE(CompTooltip, ShowcaseTooltip);
