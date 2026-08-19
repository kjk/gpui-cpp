#include "ui/slider.h"

namespace gpui {

namespace component {

Slider* Slider::New(Ctx* cx, Str id, SliderState* state) {
    Arena* a = cx->a;
    Slider* s = ArenaNew<Slider>(a);
    s->a = a;
    s->cx = cx;
    s->id = id;
    s->state = state;
    return s;
}
Slider* Slider::Reverse(bool v) {
    reverse = v;
    return this;
}
Slider* Slider::Vertical(bool v) {
    axis = v ? Axis::Vertical : Axis::Horizontal;
    return this;
}
Slider* Slider::WithAxis(Axis v) {
    axis = v;
    return this;
}
Slider* Slider::Disabled(bool v) {
    disabled = v;
    return this;
}
Slider* Slider::W(float px) {
    width = px;
    return this;
}
Slider* Slider::OnChange(Listener fn) {
    onChange = fn;
    return this;
}

static float Clamp01f(float v) {
    if (v < 0) {
        return 0;
    }
    return v > 1 ? 1 : v;
}

El* Slider::IntoEl() {
    const Theme& th = cx->theme();
    // The percentages the state already worked out from the value; a
    // single-value slider pins the low end at 0.
    float low = state ? Clamp01f(state->pctLo) : 0.f;
    float hi = state ? Clamp01f(state->pctHi) : 0.f;
    bool range = state && state->value.range;
    if (low > hi) {
        float t = low;
        low = hi;
        hi = t;
    }
    // The subscription belongs to the state, the way InputState's does: the
    // window raises the event, not the element.
    if (state && !disabled) {
        state->onChange = onChange;
    }
    const float kBar = 4.f; // h_1: the rail
    const float kThumb = 14.f;
    const float kH = 20.f;
    float w = width;
    float mid = (kH - kBar) * 0.5f;

    Rgba railBg = th.secondary;
    Rgba fillBg = disabled ? RgbaOpacity(th.primary, 0.5f) : th.primary;
    Rgba thumbBorder = disabled ? RgbaOpacity(th.primary, 0.5f) : th.primary;
    SliderState* bind = disabled ? nullptr : state;

    if (axis == Axis::Vertical) {
        // The same three parts turned on their side, filling upward from the
        // bottom of the track.
        El* vtrack = SliderTrack::New(cx, bind, axis)
                         ->W(kH)
                         ->H(w)
                         ->Click(HashClickId(id.s ? id : StrL("slider-v")));
        vtrack->Child(SliderIndicator::New(cx, bind)
                          ->Absolute()
                          ->Left(mid)
                          ->Top(0)
                          ->W(kBar)
                          ->H(w)
                          ->Radius(kBar * 0.5f)
                          ->Bg(railBg));
        float vFrom = reverse ? hi : low;
        float vTo = reverse ? 1.f : hi;
        vtrack->Child(Div(a)
                          ->Absolute()
                          ->Left(mid)
                          ->Top(w * (1.f - vTo))
                          ->W(kBar)
                          ->H(w * (vTo - vFrom))
                          ->Radius(kBar * 0.5f)
                          ->Bg(fillBg));
        for (int i = 0; i < (range ? 2 : 1); i++) {
            float at = (range && i == 0) ? low : hi;
            vtrack->Child(SliderThumb::New(cx)
                              ->Absolute()
                              ->Left((kH - kThumb) * 0.5f)
                              ->Top(w * (1.f - at) - kThumb * 0.5f)
                              ->W(kThumb)
                              ->H(kThumb)
                              ->Radius(kThumb * 0.5f)
                              ->Bg(th.background)
                              ->Border(1, thumbBorder));
        }
        return gpui::Slider::New(cx)->W(kH)->H(w)->Child(vtrack);
    }

    El* track = SliderTrack::New(cx, bind, axis)
                    ->W(w)
                    ->H(kH)
                    ->Click(HashClickId(id.s ? id : StrL("slider")));
    track->Child(SliderIndicator::New(cx, bind)
                     ->Absolute()
                     ->Top(mid)
                     ->Left(0)
                     ->W(w)
                     ->H(kBar)
                     ->Radius(kBar * 0.5f)
                     ->Bg(railBg));
    // Reversed, the filled part is what is left beyond the thumb.
    float fillFrom = reverse ? hi : low;
    float fillTo = reverse ? 1.f : hi;
    track->Child(Div(a)
                     ->Absolute()
                     ->Top(mid)
                     ->Left(w * fillFrom)
                     ->W(w * (fillTo - fillFrom))
                     ->H(kBar)
                     ->Radius(kBar * 0.5f)
                     ->Bg(fillBg));
    for (int i = 0; i < (range ? 2 : 1); i++) {
        float at = (range && i == 0) ? low : hi;
        track->Child(SliderThumb::New(cx)
                         ->Absolute()
                         ->Top((kH - kThumb) * 0.5f)
                         ->Left(w * at - kThumb * 0.5f)
                         ->W(kThumb)
                         ->H(kThumb)
                         ->Radius(kThumb * 0.5f)
                         ->Bg(th.background)
                         ->Border(1, thumbBorder));
    }
    return gpui::Slider::New(cx)->W(w)->H(kH)->Child(track);
}

} // namespace component
} // namespace gpui
