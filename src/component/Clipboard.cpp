#include "component/Clipboard.h"
#include "component/Button.h"

namespace component {

struct ClipBind {
    Func1<Str> fn;
    Str value;
};
static void FireClip(ClipBind* b) {
    b->fn.Call(b->value);
}

Clipboard* Clipboard::New(Arena* a, Str value) {
    Clipboard* c = ::New<Clipboard>(a);
    c->a = a;
    c->value = value;
    return c;
}
Clipboard* Clipboard::OnCopy(Func1<Str> fn) {
    onCopy = fn;
    return this;
}

El* Clipboard::IntoEl() {
    const Theme& th = ThemeNow();
    Button* btn = Button::New(a, StrL("clipboard"))->Icon(IconName::Copy)->Ghost()->Tooltip(StrL("Copy"));
    if (onCopy.IsValid()) {
        ClipBind* b = ::New<ClipBind>(a);
        b->fn = onCopy;
        b->value = value;
        btn->OnClick(MkFunc0(&FireClip, b));
    }
    return Div(a)->FlexRow()->ItemsCenter()->Gap(8)->Child(TextEl(a, value)->Font(13)->Fg(th.foreground))->Child(btn->IntoEl());
}

} // namespace component
