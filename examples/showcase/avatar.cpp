#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

El* ShowcaseAvatar(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    (void)app;
    struct Item {
        const char* initials;
        Rgba bg;
    };
    Item items[] = {
        {"AM", Rgb(0xf5, 0xf5, 0xf5)},
        {"JL", Rgb(0xe5, 0xe5, 0xe5)},
        {"SK", Rgb(0xd4, 0xd4, 0xd4)},
        {"+3", Rgb(0xff, 0xff, 0xff)},
    };
    El* row = Div(a)->FlexRow()->ItemsStart()->Gap(8);
    for (int i = 0; i < 4; i++) {
        El* fb = AvatarFallback::New(cx)
                     ->W(32)
                     ->H(32)
                     ->ItemsCenter()
                     ->JustifyCenter()
                     ->Bg(items[i].bg)
                     ->Child(TextEl(a, Str(items[i].initials))
                                 ->Font(12)
                                 ->Fg(Rgb(0x26, 0x26, 0x26)));
        row->Child(Avatar::New(cx)
                       ->Size(34)
                       ->Fallback(fb)
                       ->IntoEl()
                       ->ClipY()
                       ->Border(1, Rgb(0xa3, 0xa3, 0xa3))
                       ->ItemsCenter()
                       ->JustifyCenter());
    }
    return row;
}

SHOWCASE_PAGE(CompAvatar, ShowcaseAvatar);
