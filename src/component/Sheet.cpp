#include "component/Sheet.h"
#include "component/Button.h"

namespace component {

Sheet* Sheet::New(Arena* a) {
    Sheet* s = ::New<Sheet>(a);
    s->a = a;
    return s;
}
Sheet* Sheet::Title(Str s) {
    title = s;
    return this;
}
Sheet* Sheet::Open(bool v) {
    open = v;
    return this;
}
Sheet* Sheet::Body(El* e) {
    body = e;
    return this;
}
Sheet* Sheet::OnClose(Func0 fn) {
    onClose = fn;
    return this;
}

El* Sheet::IntoEl(WinSize size) {
    if (!open) {
        return Div(a);
    }
    const Theme& th = ThemeNow();
    El* surface = Div(a)->Absolute()->Top(0)->Right(0)->H(size.dipH)->W(280)->Pad(16)->FlexCol()->Gap(12)->Bg(th.background)->Border(1, th.border);
    surface->Child(TextEl(a, title)->Font(16)->Semibold()->Fg(th.foreground));
    if (body) {
        surface->Child(body);
    }
    surface->Child(Button::New(a, StrL("sheet-done"))->Label(StrL("Done"))->Primary()->OnClick(onClose)->IntoEl());
    El* overlay = Div(a)->Absolute()->Top(0)->Left(0)->W(size.dipW)->H(size.dipH)->Bg(Rgba8(0, 0, 0, 40));
    if (onClose.IsValid()) {
        overlay->OnClick(onClose)->Click(HashClickId(StrL("sheet-overlay")));
    }
    return ::Sheet::New(a)->Overlay(overlay)->Surface(surface)->IntoEl();
}

} // namespace component
