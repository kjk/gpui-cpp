#include "component/Radio.h"

namespace component {

struct RadioBind {
    Func1<bool> fn;
    bool next = true;
};

static void FireRadio(RadioBind* b) {
    b->fn.Call(b->next);
}

Radio* Radio::New(Arena* a, Str id) {
    Radio* r = ::New<Radio>(a);
    r->a = a;
    r->id = id;
    return r;
}

Radio* Radio::Label(Str s) {
    label = s;
    return this;
}
Radio* Radio::Checked(bool v) {
    checked = v;
    return this;
}
Radio* Radio::Disabled(bool v) {
    disabled = v;
    return this;
}
Radio* Radio::OnClick(Func1<bool> fn) {
    onClick = fn;
    return this;
}

El* Radio::IntoEl() {
    const Theme& th = ThemeNow();
    El* dot = Div(a)->W(14)->H(14)->Radius(7)->Border(1, th.foreground)->ItemsCenter()->JustifyCenter()->Shrink0();
    if (checked) {
        dot->Child(Div(a)->W(6)->H(6)->Radius(3)->Bg(th.primary));
    }
    El* row = ::Radio::New(a, id, disabled ? 0 : HashClickId(id))->FlexRow()->ItemsCenter()->Gap(8);
    if (onClick.IsValid() && !disabled) {
        RadioBind* b = ::New<RadioBind>(a);
        b->fn = onClick;
        row->OnClick(MkFunc0(&FireRadio, b));
    }
    row->Child(dot);
    if (label.s) {
        row->Child(TextEl(a, label)->Font(14)->Fg(disabled ? th.mutedFg : th.foreground));
    }
    return row;
}

} // namespace component
