#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

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
                      ->Id(StrL("tooltip-trigger"))
                      ->Click(HashClickId(StrL("tooltip-trigger")))
                      ->Child(btn);
    El* tip = nullptr;
    if (app->hoverId == HashClickId(StrL("tooltip-trigger"))) {
        tip = Tooltip::New(cx, StrL("example-tooltip"))
                  ->AnchorBelow(0)
                  ->Left(0)
                  ->Click(HashClickId(StrL("tooltip-trigger")))
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
