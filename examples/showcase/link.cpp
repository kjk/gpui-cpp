#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

static void OnLink(ShowcaseApp* app, Ctx* cx, const ClickEvent*) {
    (void)app;
    log(StrL("open /base/primitives/link"));
    Notify(cx);
}

// Link::styles(|s| s.disabled(..)): the look the primitive layers on only
// while the link is disabled, and which wins over the border and the colour
// chained on below it — which is what `resolve_style` promises and what the
// page used to spell out by hand for the second link.
static const LinkStyles kLinkStyles = [] {
    LinkStyles s;
    s.disabled.Border(1, ExampleRgb(0xd4d4d4)).Fg(ExampleRgb(0x737373));
    return s;
}();

El* ShowcaseLink(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    (void)app;
    return Div(a)
        ->FlexCol()
        ->W(224)
        ->Gap(8)
        ->Child(TextEl(a, StrL("Navigation is application-owned"))
                    ->Font(12)
                    ->Fg(ExampleRgb(0x171717)))
        ->Child(Link::New(cx, StrL("example-link"), false, Listen(cx, &OnLink),
                          &kLinkStyles)
                    ->W(kFill)
                    ->H(28)
                    ->PadX(12)
                    ->ItemsCenter()
                    ->Border(1, ExampleRgb(0x171717))
                    ->HoverBg(ExampleRgb(0xf5f5f5))
                    ->Child(TextEl(a, StrL("Open Link documentation  →"))
                                ->Font(12)
                                ->Fg(ExampleRgb(0x171717))))
        // The same chain as the link above; the disabled state is what makes
        // it look different, not a second set of colours here.
        ->Child(
            Link::New(cx, StrL("disabled-link"), true, Listener{}, &kLinkStyles)
                ->W(kFill)
                ->H(28)
                ->PadX(12)
                ->ItemsCenter()
                ->Border(1, ExampleRgb(0x171717))
                ->Child(TextEl(a, StrL("Disabled destination"))->Font(12)));
}

SHOWCASE_PAGE(CompLink, ShowcaseLink);
