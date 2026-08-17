#include "component/Checkbox.h"

namespace component {

struct CheckBind {
    Func1<bool> fn;
    bool next = false;
};

static void FireCheck(CheckBind* b) {
    b->fn.Call(b->next);
}

Checkbox* Checkbox::New(Arena* a, Str id) {
    Checkbox* c = ::New<Checkbox>(a);
    c->a = a;
    c->id = id;
    return c;
}

Checkbox* Checkbox::Label(Str s) {
    label = s;
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
Checkbox* Checkbox::Tooltip(Str s) {
    tooltip = s;
    return this;
}
Checkbox* Checkbox::OnClick(Func1<bool> fn) {
    onClick = fn;
    return this;
}

El* Checkbox::IntoEl() {
    const Theme& th = ThemeNow();
    float box = size == UiSize::Small ? 14.f : 16.f;
    El* ind = CheckboxIndicator::New(a)
                  ->W(box)
                  ->H(box)
                  ->Shrink0()
                  ->ItemsCenter()
                  ->JustifyCenter()
                  ->Border(1, th.foreground)
                  ->Radius(3);
    if (checked) {
        ind->Bg(th.primary)->Child(IconEl(a, IconName::Check, box - 4)->Fg(th.primaryFg));
    }
    El* row = ::Checkbox::New(a, id, disabled ? 0 : HashClickId(id))->FlexRow()->ItemsCenter()->Gap(8);
    if (onClick.IsValid() && !disabled) {
        CheckBind* b = ::New<CheckBind>(a);
        b->fn = onClick;
        b->next = !checked;
        row->OnClick(MkFunc0(&FireCheck, b));
    }
    if (tooltip.s) {
        row->Tip(tooltip);
    }
    row->Child(ind);
    if (label.s) {
        row->Child(TextEl(a, label)->Font(UiFontPx(size))->Fg(disabled ? th.mutedFg : th.foreground));
    }
    return row;
}

} // namespace component
