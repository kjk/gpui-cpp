#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

static void OnLink(ShowcaseApp* app, Ctx* cx, const ClickEvent*) {
    (void)app;
    log("open /base/primitives/link");
    Notify(cx);
}

El* ShowcaseLink(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    (void)app;
    return Div(a)
        ->FlexCol()
        ->W(224)
        ->Gap(8)
        ->Child(TextEl(a, StrL("Navigation is application-owned"))
                    ->Font(12)
                    ->Fg(Rgb(0x17, 0x17, 0x17)))
        ->Child(Link::New(cx, StrL("example-link"), false, Listen(cx, &OnLink))
                    ->W(kFill)
                    ->H(28)
                    ->PadX(12)
                    ->ItemsCenter()
                    ->Border(1, Rgb(0x17, 0x17, 0x17))
                    ->HoverBg(Rgb(0xf5, 0xf5, 0xf5))
                    ->Child(TextEl(a, StrL("Open Link documentation  →"))
                                ->Font(12)
                                ->Fg(Rgb(0x17, 0x17, 0x17))))
        ->Child(Link::New(cx, StrL("disabled-link"), true)
                    ->W(kFill)
                    ->H(28)
                    ->PadX(12)
                    ->ItemsCenter()
                    ->Border(1, Rgb(0xd4, 0xd4, 0xd4))
                    ->Child(TextEl(a, StrL("Disabled destination"))
                                ->Font(12)
                                ->Fg(Rgb(0x73, 0x73, 0x73))));
}

SHOWCASE_PAGE(CompLink, ShowcaseLink);
