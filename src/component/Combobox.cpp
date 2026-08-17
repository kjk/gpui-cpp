#include "component/Combobox.h"

namespace component {

struct CbBind {
    Func1<int> fn;
    int index = 0;
};
static void FireCb(CbBind* b) {
    b->fn.Call(b->index);
}

Combobox* Combobox::New(Arena* a, Str id) {
    Combobox* c = ::New<Combobox>(a);
    c->a = a;
    c->id = id;
    return c;
}
Combobox* Combobox::Option(Str s) {
    if (n < 8) {
        options[n++] = s;
    }
    return this;
}
Combobox* Combobox::Selected(Str s) {
    selected = s;
    return this;
}
Combobox* Combobox::Open(bool v) {
    open = v;
    return this;
}
Combobox* Combobox::Query(LineInput* q) {
    query = q;
    return this;
}
Combobox* Combobox::OnChange(Func1<int> fn) {
    onChange = fn;
    return this;
}
Combobox* Combobox::OnToggle(Func0 fn) {
    onToggle = fn;
    return this;
}

El* Combobox::IntoEl() {
    const Theme& th = ThemeNow();
    El* trigger = Div(a)
                      ->H(28)
                      ->PadX(8)
                      ->ItemsCenter()
                      ->JustifyBetween()
                      ->Border(1, th.border)
                      ->Child(TextEl(a, selected.s ? selected : StrL("Select"))->Font(13)->Fg(th.foreground))
                      ->Child(IconEl(a, IconName::ChevronDown, 14)->Fg(th.mutedFg));
    BindClick(trigger, id, onToggle);
    El* pop = nullptr;
    if (open) {
        pop = Div(a)->FlexCol()->Pad(4)->Border(1, th.border)->Bg(th.background);
        if (query) {
            pop->Child(InputBase::New(a, StrL("cb-q"), HashClickId(StrL("cb-q")))
                           ->H(28)
                           ->PadX(8)
                           ->ItemsCenter()
                           ->Border(1, th.border)
                           ->Child(::Input::New(a, query)));
        }
        for (int i = 0; i < n; i++) {
            El* row = Div(a)->H(28)->PadX(8)->ItemsCenter()->HoverBg(th.muted)->Child(
                TextEl(a, options[i])->Font(13)->Fg(th.foreground));
            if (onChange.IsValid()) {
                CbBind* b = ::New<CbBind>(a);
                b->fn = onChange;
                b->index = i;
                BindClick(row, options[i], MkFunc0(&FireCb, b));
            }
            pop->Child(row);
        }
    }
    El* root = ::Combobox::New(a, id)->W(224)->Child(trigger);
    return Popup::New(a, StrL("combo-pop"), root)->Content(pop)->IntoEl();
}

} // namespace component
