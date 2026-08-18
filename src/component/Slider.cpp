#include "component/Slider.h"

namespace gpui {

namespace component {

Slider* Slider::New(Ctx* cx) {
    Arena* a = cx->a;
    Slider* s = ArenaNew<Slider>(a);
    s->a = a;
    s->cx = cx;
    return s;
}
Slider* Slider::New(Ctx* cx, Str id) {
    Slider* s = New(cx);
    s->id = id;
    return s;
}
Slider* Slider::Value(float v) {
    value = v;
    return this;
}
Slider* Slider::Range(float low, float high) {
    lo = low;
    value = high;
    range = true;
    return this;
}
Slider* Slider::Reverse(bool v) {
    reverse = v;
    return this;
}
Slider* Slider::Vertical(bool v) {
    vertical = v;
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
    float hi = Clamp01f(value);
    float low = range ? Clamp01f(lo) : 0.f;
    if (low > hi) {
        float t = low;
        low = hi;
        hi = t;
    }
    const float kBar = 4.f; // h_1: the rail
    const float kThumb = 14.f;
    const float kH = 20.f;
    float w = width;
    float mid = (kH - kBar) * 0.5f;

    Rgba railBg = th.secondary;
    Rgba fillBg = disabled ? RgbaOpacity(th.primary, 0.5f) : th.primary;
    Rgba thumbBorder = disabled ? RgbaOpacity(th.primary, 0.5f) : th.primary;

    if (vertical) {
        // The same three parts turned on their side, filling upward from the
        // bottom of the track.
        El* vtrack = SliderTrack::New(cx)->W(kH)->H(w);
        vtrack->Child(Div(a)
                          ->Absolute()
                          ->Left(mid)
                          ->Top(0)
                          ->W(kBar)
                          ->H(w)
                          ->Radius(kBar * 0.5f)
                          ->Bg(railBg));
        float vFrom = reverse ? hi : low;
        float vTo = reverse ? 1.f : hi;
        vtrack->Child(SliderIndicator::New(cx)
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
        El* vroot =
            gpui::Slider::New(cx, HashClickId(id.s ? id : StrL("slider-v")))
                ->W(kH)
                ->H(w)
                ->Child(vtrack);
        if (onChange.IsValid() && !disabled) {
            vroot->OnClick(onChange);
        }
        return vroot;
    }

    El* track = SliderTrack::New(cx)->W(w)->H(kH);
    track->Child(Div(a)
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
    track->Child(SliderIndicator::New(cx)
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
    El* root = gpui::Slider::New(cx, HashClickId(id.s ? id : StrL("slider")))
                   ->W(w)
                   ->H(kH)
                   ->Child(track);
    if (onChange.IsValid() && !disabled) {
        root->OnClick(onChange);
    }
    return root;
}

} // namespace component
} // namespace gpui
