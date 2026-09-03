#include "ui/switch.h"
#include "base/motion.h"

namespace gpui {

namespace component {

// switch.rs no longer names a spring of its own: the thumb travels under the
// theme's `spring_move`, the policy everything that goes from one place to
// another shares, and the tolerance it carries is already the tenth of a
// pixel a travel in pixels wants.

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
    const Theme& th = ThemeNow(cx->app);
    Background on = hasColor ? Background(color) : th.tokens.primary;
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
    // The unstyled parts own semantic-state priority. The unchecked fill is
    // the instance baseline; checked replaces it and disabled replaces that
    // only for the checked case, exactly as the conditional Rust style does.
    SwitchTrackStyles trackStyles;
    trackStyles.Checked(StateStyle().Bg(on));
    if (checked) {
        trackStyles.Disabled(StateStyle().Bg(BackgroundOpacity(on, 0.5f)));
    }
    SwitchThumbStyles thumbStyles;
    thumbStyles.Disabled(
        StateStyle().Bg(BackgroundOpacity(th.tokens.switchThumb, 0.35f)));
    // Rust builds the track's id from `(id, "track")`, so the part is named
    // apart from the switch it sits in.
    El* track = SwitchTrack::New(cx, StrDup(a, fmt("%s-track", id)), checked,
                                 disabled, &trackStyles)
                    ->W(trackW)
                    ->H(trackH)
                    ->Pad(2)
                    ->Radius(trackH * 0.5f)
                    ->ItemsCenter();
    if (!checked) {
        track->Bg(th.tokens.secondary);
    }
    // The thumb slides rather than jumping: Rust animates `left` from one end
    // to the other over 150 ms whenever the checked flag turns over. A
    // disabled switch does not animate there, and does not here.
    float inset = 2.f;
    float maxX = trackW - thumb - inset * 2;
    float x = checked ? maxX : 0.f;
    if (!disabled) {
        // spring_move: a switch is toggled again long before the thumb has
        // finished travelling, and a spring turns it around from where it is
        // rather than restarting the curve.
        x = SpringValue(cx, MotionId(id, StrL("switch-thumb")), x,
                        th.motion.springMove);
    }
    // Absolutely placed, since what moves is an offset rather than which end
    // of the track the thumb is packed against.
    El* thumbEl = SwitchThumb::New(cx, checked, disabled, &thumbStyles)
                      ->Absolute()
                      ->Left(inset + x)
                      ->Top(inset)
                      ->W(thumb)
                      ->H(thumb)
                      ->Radius(thumb * 0.5f);
    if (!disabled) {
        thumbEl->Bg(th.tokens.switchThumb);
    }
    track->Child(thumbEl);
    // gpui_base::Switch owns identity, focus and activation, and hands the
    // handler the value the activation produces.
    El* root = gpui::Switch::New(cx, id, checked, disabled, onClick, nullptr,
                                 nullptr, label)
                   ->FlexRow()
                   ->ItemsCenter()
                   ->Gap(8);
    root->Child(track);
    if (label.s) {
        // text_sm below Medium, text_base at and above it — and the label
        // keeps its colour when the switch is disabled; only the track dims.
        float labelFont =
            (size == UiSize::XSmall || size == UiSize::Small) ? 14.f : 16.f;
        root->Child(TextEl(a, label)->Font(labelFont)->Fg(th.foreground));
    }
    return root;
}

} // namespace component
} // namespace gpui
