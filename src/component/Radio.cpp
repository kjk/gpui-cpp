#include "component/Radio.h"

namespace gpui {

namespace component {

struct RadioBind {
    Func1<bool> fn;
    bool next = true;
};

static void FireRadio(RadioBind* b) {
    b->fn.Call(b->next);
}

Radio* Radio::New(Arena* a, Str id) {
    Radio* r = ArenaNew<Radio>(a);
    r->a = a;
    r->id = id;
    return r;
}

Radio* Radio::Label(Str s) {
    label = s;
    return this;
}
Radio* Radio::Hint(Str s) {
    hint = s;
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
Radio* Radio::WithSize(UiSize s) {
    size = s;
    return this;
}
Radio* Radio::OnClick(Func1<bool> fn) {
    onClick = fn;
    return this;
}

El* Radio::IntoEl() {
    const Theme& th = ThemeNow();
    El* dot = Div(a)
                  ->W(14)
                  ->H(14)
                  ->Radius(7)
                  ->Border(1, th.foreground)
                  ->ItemsCenter()
                  ->JustifyCenter()
                  ->Shrink0();
    if (checked) {
        dot->Child(Div(a)->W(6)->H(6)->Radius(3)->Bg(th.primary));
    }
    El* row = gpui::Radio::New(a, id, disabled ? 0 : HashClickId(id))
                  ->FlexRow()
                  ->ItemsCenter()
                  ->Gap(8);
    if (onClick.IsValid() && !disabled) {
        RadioBind* b = ArenaNew<RadioBind>(a);
        b->fn = onClick;
        row->OnClick(MkFunc0(&FireRadio, b));
    }
    row->Child(dot);
    if (label.s || hint.s) {
        El* col = Div(a)->FlexCol()->Gap(2);
        if (label.s) {
            col->Child(TextEl(a, label)
                           ->Font(UiFontPx(size))
                           ->Fg(disabled ? th.mutedFg : th.foreground));
        }
        if (hint.s) {
            col->Child(TextEl(a, hint)->Font(12)->Fg(th.mutedFg)->Wrap());
        }
        row->Child(col);
    }
    return row;
}

} // namespace component
} // namespace gpui
