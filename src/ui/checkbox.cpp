#include "ui/checkbox.h"
#include "base/motion.h"

namespace gpui {

namespace component {

// checkbox.rs: the tick takes 0.25 s to arrive, and as long to leave.
static const float kCheckboxMotionMs = 250.f;

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
    const Theme& th = cx->theme();
    float box = size == UiSize::Small ? 14.f : 16.f;
    // An unchecked box carries the input border, a checked one the primary
    // color, and a disabled one either at half strength.
    Rgba mark = checked ? th.primary : th.inputBorder;
    if (disabled) {
        mark = RgbaOpacity(mark, 0.5f);
    }
    float radius = th.radius < 4.f ? th.radius : 4.f;
    El* ind = CheckboxIndicator::New(cx)
                  ->W(box)
                  ->H(box)
                  ->Shrink0()
                  ->ItemsCenter()
                  ->JustifyCenter()
                  ->Border(1, mark)
                  ->Radius(radius);
    if (checked) {
        ind->Bg(mark);
    }
    // checkbox.rs fades the tick in over 0.25 s, and back out again when the
    // box is cleared. There is no element opacity here, so the fade is the
    // tick's own alpha — the mark is the only thing it paints.
    float on = checked ? 1.f : 0.f;
    if (!disabled) {
        on = MotionValue(cx, MotionId(id, StrL("checkbox-tick")), on,
                         MotionNew(kCheckboxMotionMs));
    }
    if (on > 0.01f) {
        Rgba tick = disabled ? RgbaOpacity(th.primaryFg, 0.5f) : th.primaryFg;
        ind->Child(IconEl(a, IconName::Check, box - 4)
                       ->Fg(RgbaOpacity(tick, on)));
    }
    // gpui_base::Checkbox owns identity, focus and activation. It hands the
    // handler the state the activation produces; the themed checkbox is
    // boolean, and CheckboxState::Unchecked / Checked are 0 and 1, so what
    // the caller reads is the `!checked` Rust passes on.
    CheckboxState state =
        checked ? CheckboxState::Checked : CheckboxState::Unchecked;
    El* row = gpui::Checkbox::New(cx, id, state, disabled, onClick)
                  ->FlexRow()
                  ->ItemsCenter()
                  ->Gap(8);
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
