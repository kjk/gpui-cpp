#include "component/Checkbox.h"

namespace gpui {

namespace component {

struct CheckBind {
    Func1<bool> fn;
    bool next = false;
};

static void FireCheck(CheckBind* b) {
    b->fn.Call(b->next);
}

Checkbox* Checkbox::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    Checkbox* c = ArenaNew<Checkbox>(a);
    c->a = a;
    c->cx = cx;
    c->id = id;
    return c;
}

Checkbox* Checkbox::Label(Str s) {
    label = s;
    return this;
}
Checkbox* Checkbox::Hint(Str s) {
    hint = s;
    return this;
}
Checkbox* Checkbox::Checked(bool v) {
    checked = v;
    return this;
}
Checkbox* Checkbox::Disabled(bool v) {
    disabled = v;
    return this;
}
Checkbox* Checkbox::WithSize(UiSize s) {
    size = s;
    return this;
}
Checkbox* Checkbox::W(float v) {
    w = v;
    return this;
}
Checkbox* Checkbox::Tooltip(Str s) {
    tooltip = s;
    return this;
}
Checkbox* Checkbox::OnClick(Listener fn) {
    onClick = fn;
    return this;
}

El* Checkbox::IntoEl() {
    const Theme& th = ThemeNow();
    float box = size == UiSize::Small ? 14.f : 16.f;
    El* ind = CheckboxIndicator::New(cx)
                  ->W(box)
                  ->H(box)
                  ->Shrink0()
                  ->ItemsCenter()
                  ->JustifyCenter()
                  ->Border(1, th.foreground)
                  ->Radius(3);
    if (checked) {
        ind->Bg(th.primary)
            ->Child(IconEl(a, IconName::Check, box - 4)->Fg(th.primaryFg));
    }
    El* row = gpui::Checkbox::New(cx, id, disabled ? 0 : HashClickId(id))
                  ->FlexRow()
                  ->ItemsCenter()
                  ->Gap(8);
    if (onClick.IsValid() && !disabled) {
        row->OnClick(ListenerArg(onClick, !checked));
    }
    if (tooltip.s) {
        row->Tip(tooltip);
    }
    row->Child(ind);
    if (w > 0) {
        row->W(w);
    }
    if (label.s || hint.s) {
        El* col = Div(a)->FlexCol()->Gap(2);
        if (label.s) {
            col->Child(TextEl(a, label)
                           ->Font(UiFontPx(size))
                           ->Fg(disabled ? th.mutedFg : th.foreground)
                           ->Wrap());
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
