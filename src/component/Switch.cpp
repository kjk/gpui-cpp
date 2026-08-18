#include "component/Switch.h"

namespace gpui {

namespace component {

Switch* Switch::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    Switch* s = ArenaNew<Switch>(a);
    s->a = a;
    s->cx = cx;
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
Switch* Switch::OnClick(Listener fn) {
    onClick = fn;
    return this;
}

El* Switch::IntoEl() {
    const Theme& th = cx->theme();
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
    // Disabled halves the checked track and dims the thumb, the way
    // disabled_checked_bg and the thumb's disabled style do.
    Rgba trackBg = checked ? on : th.secondary;
    if (disabled && checked) {
        trackBg = RgbaOpacity(trackBg, 0.5f);
    }
    Rgba thumbBg = disabled ? RgbaOpacity(th.background, 0.35f) : th.background;
    El* track = SwitchTrack::New(cx, id)
                    ->W(trackW)
                    ->H(trackH)
                    ->Pad(2)
                    ->Radius(trackH * 0.5f)
                    ->Bg(trackBg)
                    ->ItemsCenter();
    if (checked) {
        track->JustifyEnd();
    } else {
        track->JustifyStart();
    }
    track->Child(SwitchThumb::New(cx)
                     ->W(thumb)
                     ->H(thumb)
                     ->Radius(thumb * 0.5f)
                     ->Bg(thumbBg));
    El* root = gpui::Switch::New(cx, id, disabled ? 0 : HashClickId(id))
                   ->FlexRow()
                   ->ItemsCenter()
                   ->Gap(8);
    if (onClick.IsValid() && !disabled) {
        root->OnClick(ListenerArg(onClick, !checked));
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
