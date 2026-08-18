#include "component/Select.h"

namespace gpui {

namespace component {

struct SelBind {
    Func1<int> fn;
    int index = 0;
};
static void FireSel(SelBind* b) {
    b->fn.Call(b->index);
}

Select* Select::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    Select* s = ArenaNew<Select>(a);
    s->a = a;
    s->cx = cx;
    s->id = id;
    return s;
}
Select* Select::Option(Str s) {
    if (n < 8) {
        options[n++] = s;
    }
    return this;
}
Select* Select::Selected(int i) {
    selected = i;
    return this;
}
Select* Select::Open(bool v) {
    open = v;
    return this;
}
Select* Select::OnChange(Listener fn) {
    onChange = fn;
    return this;
}
Select* Select::OnToggle(Listener fn) {
    onToggle = fn;
    return this;
}

El* Select::IntoEl() {
    const Theme& th = cx->theme();
    int sel = selected;
    if (sel < 0 || sel >= n) {
        sel = 0;
    }
    El* trigger =
        Div(a)
            ->H(28)
            ->PadX(8)
            ->ItemsCenter()
            ->JustifyBetween()
            ->Border(1, th.foreground)
            ->Child(TextEl(a, n ? options[sel] : StrL("Select"))
                        ->Font(13)
                        ->Fg(th.foreground))
            ->Child(IconEl(a, IconName::ChevronDown, 14)->Fg(th.mutedFg));
    BindClick(trigger, id, onToggle);
    El* opts = nullptr;
    if (open) {
        opts = Div(a)
                   ->FlexCol()
                   ->Pad(4)
                   ->Border(1, th.foreground)
                   ->Bg(th.background);
        for (int i = 0; i < n; i++) {
            El* row = Div(a)->PadX(8)->PadY(4)->HoverBg(th.muted)->Child(
                TextEl(a, options[i])->Font(13)->Fg(th.foreground));
            if (onChange.IsValid()) {
                BindClick(row, options[i], ListenerArg(onChange, i));
            }
            opts->Child(row);
        }
    }
    El* root = gpui::Select::New(cx, id)->W(224)->Child(trigger);
    return Popup::New(cx, StrL("select-popup"), root)->Content(opts)->IntoEl();
}

} // namespace component
} // namespace gpui
