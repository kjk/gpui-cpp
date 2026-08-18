#include "component/Clipboard.h"
#include "component/Button.h"

namespace gpui {

namespace component {

struct ClipBind {
    Func1<Str> fn;
    Str value;
};
static void FireClip(ClipBind* b) {
    b->fn.Call(b->value);
}

Clipboard* Clipboard::New(Ctx* cx, Str value) {
    Arena* a = cx->a;
    Clipboard* c = ArenaNew<Clipboard>(a);
    c->a = a;
    c->cx = cx;
    c->value = value;
    return c;
}
Clipboard* Clipboard::OnCopy(Listener fn) {
    onCopy = fn;
    return this;
}

El* Clipboard::IntoEl() {
    const Theme& th = cx->theme();
    Button* btn = Button::New(cx, StrL("clipboard"))
                      ->Icon(IconName::Copy)
                      ->Ghost()
                      ->Tooltip(StrL("Copy"));
    if (onCopy.IsValid()) {
        btn->OnClick(onCopy);
    }
    return Div(a)
        ->FlexRow()
        ->ItemsCenter()
        ->Gap(8)
        ->Child(TextEl(a, value)->Font(13)->Fg(th.foreground))
        ->Child(btn->IntoEl());
}

} // namespace component
} // namespace gpui
