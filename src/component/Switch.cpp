#include "component/Switch.h"

namespace component {

struct SwitchBind {
    Func1<bool> fn;
    bool next = false;
};

static void FireSwitch(SwitchBind* b) {
    b->fn.Call(b->next);
}

Switch* Switch::New(Arena* a, Str id) {
    Switch* s = ::New<Switch>(a);
    s->a = a;
    s->id = id;
    return s;
}

Switch* Switch::Label(Str s) {
    label = s;
    return this;
}
Switch* Switch::Checked(bool v) {
    checked = v;
    return this;
}
Switch* Switch::Disabled(bool v) {
    disabled = v;
    return this;
}
Switch* Switch::Color(Rgba c) {
    color = c;
    hasColor = true;
    return this;
}
Switch* Switch::OnClick(Func1<bool> fn) {
    onClick = fn;
    return this;
}

El* Switch::IntoEl() {
    const Theme& th = ThemeNow();
    Rgba on = hasColor ? color : th.primary;
    El* track = SwitchTrack::New(a, id)
                    ->W(36)
                    ->H(20)
                    ->Pad(2)
                    ->Radius(10)
                    ->Bg(checked ? on : th.secondary)
                    ->ItemsCenter();
    if (checked) {
        track->JustifyEnd();
    } else {
        track->JustifyStart();
    }
    track->Child(
        SwitchThumb::New(a)->W(16)->H(16)->Radius(8)->Bg(th.background));
    El* root = ::Switch::New(a, id, disabled ? 0 : HashClickId(id))
                   ->FlexRow()
                   ->ItemsCenter()
                   ->Gap(8);
    if (onClick.IsValid() && !disabled) {
        SwitchBind* b = ::New<SwitchBind>(a);
        b->fn = onClick;
        b->next = !checked;
        root->OnClick(MkFunc0(&FireSwitch, b));
    }
    root->Child(track);
    if (label.s) {
        root->Child(TextEl(a, label)->Font(14)->Fg(disabled ? th.mutedFg
                                                            : th.foreground));
    }
    return root;
}

} // namespace component
