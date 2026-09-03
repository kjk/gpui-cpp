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
        {"AM", ExampleRgb(0xf5f5f5)},
        {"JL", ExampleRgb(0xe5e5e5)},
        {"SK", ExampleRgb(0xd4d4d4)},
        {"+3", ExampleRgb(0xffffff)},
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
                                 ->Fg(ExampleRgb(0x262626)));
        row->Child(Avatar::New(cx)
                       ->Size(34)
                       ->Fallback(fb)
                       ->IntoEl()
                       ->ClipY()
                       ->Border(1, ExampleRgb(0xa3a3a3))
                       ->ItemsCenter()
                       ->JustifyCenter());
    }
    return row;
}

SHOWCASE_PAGE(CompAvatar, ShowcaseAvatar);
