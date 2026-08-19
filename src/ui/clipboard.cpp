#include "ui/clipboard.h"
#include "ui/button.h"

namespace gpui {

namespace component {

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
    // A Clipboard is just the ghost icon button; `value` is what it copies,
    // not something it shows. The caller renders any label next to it.
    Button* btn = Button::New(cx, StrL("clipboard"))
                      ->Icon(IconName::Copy)
                      ->Ghost()
                      ->WithSize(UiSize::XSmall)
                      ->Tooltip(StrL("Copy"));
    if (onCopy.IsValid()) {
        btn->OnClick(onCopy);
    }
    return btn->IntoEl();
}

} // namespace component
} // namespace gpui
