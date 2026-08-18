#include "component/Combobox.h"

namespace gpui {

namespace component {

struct CbBind {
    Func1<int> fn;
    int index = 0;
};
static void FireCb(CbBind* b) {
    b->fn.Call(b->index);
}

Combobox* Combobox::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    Combobox* c = ArenaNew<Combobox>(a);
    c->a = a;
    c->cx = cx;
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
Combobox* Combobox::OnChange(Listener fn) {
    onChange = fn;
    return this;
}
Combobox* Combobox::OnToggle(Listener fn) {
    onToggle = fn;
    return this;
}

El* Combobox::IntoEl() {
    const Theme& th = ThemeNow();
    El* trigger =
        Div(a)
            ->H(28)
            ->PadX(8)
            ->ItemsCenter()
            ->JustifyBetween()
            ->Border(1, th.border)
            ->Child(TextEl(a, selected.s ? selected : StrL("Select"))
                        ->Font(13)
                        ->Fg(th.foreground))
            ->Child(IconEl(a, IconName::ChevronDown, 14)->Fg(th.mutedFg));
    BindClick(trigger, id, onToggle);
    El* pop = nullptr;
    if (open) {
        pop =
            Div(a)->FlexCol()->Pad(4)->Border(1, th.border)->Bg(th.background);
        if (query) {
            pop->Child(
                InputBase::New(cx, StrL("cb-q"), HashClickId(StrL("cb-q")))
                    ->H(28)
                    ->PadX(8)
                    ->ItemsCenter()
                    ->Border(1, th.border)
                    ->Child(gpui::Input::New(cx, query)));
        }
        for (int i = 0; i < n; i++) {
            El* row =
                Div(a)->H(28)->PadX(8)->ItemsCenter()->HoverBg(th.muted)->Child(
                    TextEl(a, options[i])->Font(13)->Fg(th.foreground));
            if (onChange.IsValid()) {
                BindClick(row, options[i], ListenerArg(onChange, i));
            }
            pop->Child(row);
        }
    }
    El* root = gpui::Combobox::New(cx, id)->W(224)->Child(trigger);
    return Popup::New(cx, StrL("combo-pop"), root)->Content(pop)->IntoEl();
}

} // namespace component
} // namespace gpui
