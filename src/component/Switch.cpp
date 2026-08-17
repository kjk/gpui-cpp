#include "component/Switch.h"

namespace gpui {

namespace component {

struct SwitchBind {
    Func1<bool> fn;
    bool next = false;
};

static void FireSwitch(SwitchBind* b) {
    b->fn.Call(b->next);
}

Switch* Switch::New(Arena* a, Str id) {
    Switch* s = ArenaNew<Switch>(a);
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
Switch* Switch::WithSize(UiSize s) {
    size = s;
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
    float trackW = 36;
    float trackH = 20;
    float thumb = 16;
    if (size == UiSize::Small || size == UiSize::XSmall) {
        trackW = 28;
        trackH = 16;
        thumb = 12;
    } else if (size == UiSize::Large) {
        trackW = 44;
        trackH = 24;
        thumb = 20;
    }
    El* track = SwitchTrack::New(a, id)
                    ->W(trackW)
                    ->H(trackH)
                    ->Pad(2)
                    ->Radius(trackH * 0.5f)
                    ->Bg(checked ? on : th.secondary)
                    ->ItemsCenter();
    if (checked) {
        track->JustifyEnd();
    } else {
        track->JustifyStart();
    }
    track->Child(SwitchThumb::New(a)
                     ->W(thumb)
                     ->H(thumb)
                     ->Radius(thumb * 0.5f)
                     ->Bg(th.background));
    El* root = gpui::Switch::New(a, id, disabled ? 0 : HashClickId(id))
                   ->FlexRow()
                   ->ItemsCenter()
                   ->Gap(8);
    if (onClick.IsValid() && !disabled) {
        SwitchBind* b = ArenaNew<SwitchBind>(a);
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
} // namespace gpui
