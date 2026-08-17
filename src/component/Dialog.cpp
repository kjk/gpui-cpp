#include "component/Dialog.h"
#include "component/Button.h"

namespace gpui {

namespace component {

Dialog* Dialog::New(Ctx* cx) {
    Arena* a = cx->a;
    Dialog* d = ArenaNew<Dialog>(a);
    d->a = a;
    d->cx = cx;
    return d;
}
Dialog* Dialog::Title(Str s) {
    title = s;
    return this;
}
Dialog* Dialog::Description(Str s) {
    description = s;
    return this;
}
Dialog* Dialog::Open(bool v) {
    open = v;
    return this;
}
Dialog* Dialog::Body(El* e) {
    body = e;
    return this;
}
Dialog* Dialog::OnClose(Func0 fn) {
    onClose = fn;
    return this;
}
Dialog* Dialog::OnOk(Func0 fn) {
    onOk = fn;
    return this;
}

El* Dialog::IntoEl(WinSize size) {
    if (!open) {
        return Div(a);
    }
    const Theme& th = ThemeNow();
    El* panel = Div(a)
                    ->W(360)
                    ->Pad(16)
                    ->FlexCol()
                    ->Gap(8)
                    ->Bg(th.background)
                    ->Border(1, th.border)
                    ->Radius(th.radius);
    panel->Child(DialogTitle::New(cx)->Child(
        TextEl(a, title)->Font(16)->Semibold()->Fg(th.foreground)));
    if (description.s) {
        panel->Child(DialogDescription::New(cx)->Child(
            TextEl(a, description)->Font(13)->Fg(th.mutedFg)->Wrap()));
    }
    if (body) {
        panel->Child(body);
    }
    El* actions = Div(a)->FlexRow()->JustifyEnd()->Gap(8);
    actions->Child(Button::New(cx, StrL("dialog-cancel"))
                       ->Label(StrL("Cancel"))
                       ->OnClick(onClose)
                       ->IntoEl());
    actions->Child(Button::New(cx, StrL("dialog-ok"))
                       ->Label(StrL("OK"))
                       ->Primary()
                       ->OnClick(onOk)
                       ->IntoEl());
    panel->Child(actions);
    El* backdrop = DialogBackdrop::New(cx)
                       ->Absolute()
                       ->Top(0)
                       ->Left(0)
                       ->W(size.dipW)
                       ->H(size.dipH)
                       ->Bg(Rgba8(0, 0, 0, 51));
    if (onClose.IsValid()) {
        backdrop->OnClick(onClose)->Click(HashClickId(StrL("dialog-backdrop")));
    }
    El* popup = DialogPopup::New(cx)
                    ->Absolute()
                    ->Top(0)
                    ->Left(0)
                    ->W(size.dipW)
                    ->H(size.dipH)
                    ->ItemsCenter()
                    ->JustifyCenter()
                    ->Child(panel);
    return gpui::Dialog::New(cx)->Backdrop(backdrop)->Popup(popup)->IntoEl();
}

} // namespace component
} // namespace gpui
