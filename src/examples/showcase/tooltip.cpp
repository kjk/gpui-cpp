#include "Showcase.h"
#include "ui/Button.h"
#include "ui/Popup.h"
#include "ui/Tooltip.h"

enum { ClickTooltip = 570 };

El* ShowcaseTooltip(ShowcaseApp* app, Arena* a) {
    // Rust wraps the button in a hover target; the button itself has no hover fill.
    El* btn = Button::New(a, StrL("tooltip-anchor"), 0)
                  ->H(28)
                  ->PadX(8)
                  ->ItemsCenter()
                  ->JustifyCenter()
                  ->Border(1, Rgb(0x17, 0x17, 0x17))
                  ->Bg(Rgb(0xff, 0xff, 0xff))
                  ->Child(TextEl(a, StrL("Command menu"))->Font(12)->Fg(Rgb(0x17, 0x17, 0x17)));
    El* trigger = Div(a)->Id(StrL("tooltip-trigger"))->Click(ClickTooltip)->Child(btn);
    El* tip = nullptr;
    if (app->hoverId == ClickTooltip) {
        tip = Tooltip::New(a, StrL("example-tooltip"))
                  ->AnchorBelow(0)
                  ->Left(0)
                  ->Click(ClickTooltip)
                  ->PadX(8)
                  ->H(28)
                  ->ItemsCenter()
                  ->JustifyCenter()
                  ->Border(1, Rgb(0x17, 0x17, 0x17))
                  ->Bg(Rgb(0x17, 0x17, 0x17))
                  ->Child(TextEl(a, StrL("Open command menu · \xE2\x8C\x98K"))->Font(12)->Fg(Rgb(0xff, 0xff, 0xff)));
    }
    return Popup::New(a, StrL("example-tooltip-popup"), trigger)->Content(tip)->IntoEl();
}

void ShowcaseTooltipClick(ShowcaseApp* app, int id) {
    (void)app;
    (void)id;
}

SHOWCASE_PAGE(CompTooltip, ShowcaseTooltip, ShowcaseTooltipClick);

