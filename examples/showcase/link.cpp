#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

enum {
    ClickLink = 390
};

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
        ->Child(Link::New(a, StrL("example-link"), ClickLink)
                    ->W(kFill)
                    ->H(28)
                    ->PadX(12)
                    ->ItemsCenter()
                    ->Border(1, Rgb(0x17, 0x17, 0x17))
                    ->HoverBg(Rgb(0xf5, 0xf5, 0xf5))
                    ->Child(TextEl(a, StrL("Open Link documentation  →"))
                                ->Font(12)
                                ->Fg(Rgb(0x17, 0x17, 0x17))))
        ->Child(Link::New(a, StrL("disabled-link"))
                    ->W(kFill)
                    ->H(28)
                    ->PadX(12)
                    ->ItemsCenter()
                    ->Border(1, Rgb(0xd4, 0xd4, 0xd4))
                    ->Child(TextEl(a, StrL("Disabled destination"))
                                ->Font(12)
                                ->Fg(Rgb(0x73, 0x73, 0x73))));
}

void ShowcaseLinkClick(ShowcaseApp* app, int id) {
    (void)app;
    if (id == ClickLink) {
        log("open /base/primitives/link");
    }
}

SHOWCASE_PAGE(CompLink, ShowcaseLink, ShowcaseLinkClick);
