#include "ui/switch.h"
#include "base/motion.h"

namespace gpui {

namespace component {

// switch.rs: the thumb's move takes 0.15 s.
static const float kSwitchMotionMs = 150.f;

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
    // Rust builds the track's id from `(id, "track")`, so the part is named
    // apart from the switch it sits in.
    El* track = SwitchTrack::New(cx, StrDup(a, fmt("%s-track", id)))
                    ->W(trackW)
                    ->H(trackH)
                    ->Pad(2)
                    ->Radius(trackH * 0.5f)
                    ->Bg(trackBg)
                    ->ItemsCenter();
    // The thumb slides rather than jumping: Rust animates `left` from one end
    // to the other over 150 ms whenever the checked flag turns over. A
    // disabled switch does not animate there, and does not here.
    float inset = 2.f;
    float maxX = trackW - thumb - inset * 2;
    float x = checked ? maxX : 0.f;
    if (!disabled) {
        x = MotionValue(cx, MotionId(id, StrL("switch-thumb")), x,
                        MotionNew(kSwitchMotionMs));
    }
    // Absolutely placed, since what moves is an offset rather than which end
    // of the track the thumb is packed against.
    track->Child(SwitchThumb::New(cx)
                     ->Absolute()
                     ->Left(inset + x)
                     ->Top(inset)
                     ->W(thumb)
                     ->H(thumb)
                     ->Radius(thumb * 0.5f)
                     ->Bg(thumbBg));
    // gpui_base::Switch owns identity, focus and activation, and hands the
    // handler the value the activation produces.
    El* root = gpui::Switch::New(cx, id, checked, disabled, onClick)
                   ->FlexRow()
                   ->ItemsCenter()
                   ->Gap(8);
    root->Child(track);
    if (label.s) {
        root->Child(TextEl(a, label)->Font(14)->Fg(disabled ? th.mutedFg
                                                            : th.foreground));
    }
    return root;
}

} // namespace component
} // namespace gpui
