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

Slider* Slider::WFill() {
    width = kFill;
    return this;
}
Slider* Slider::Bg(Rgba c) {
    bar = c;
    hasBar = true;
    return this;
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

    Background railBg = th.tokens.secondary;
    Background fillBg = disabled ? BackgroundOpacity(th.tokens.primary, 0.5f)
                                 : th.tokens.primary;
    Rgba thumbBorder = disabled ? RgbaOpacity(th.primary, 0.5f) : th.primary;
    // bar_color: the rail at 20%, the fill whole, the thumb's ring at 50%.
    if (hasBar) {
        railBg = RgbaOpacity(bar, 0.2f);
        Rgba full = disabled ? RgbaOpacity(bar, 0.5f) : bar;
        fillBg = full;
        thumbBorder = RgbaOpacity(bar, 0.5f);
    }
    SliderState* bind = disabled ? nullptr : state;

    if (axis == Axis::Vertical) {
        // The same three parts turned on their side, filling upward from the
        // bottom of the track.
        int vid = HashClickId(id.s ? id : StrL("slider-v"));
        El* vtrack = SliderTrack::New(cx, bind, axis)->W(kH)->H(w)->Click(vid);
        // Focusable, so the arrows can reach it: `on_a11y_action(Increment |
        // Decrement)` is what they stand in for, and a track that cannot take
        // focus has no way to hear them.
        if (!disabled) {
            vtrack->FocusId(vid);
        }
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
                              ->Bg(th.tokens.sliderThumb)
                              ->Border(1, thumbBorder));
        }
        return gpui::Slider::New(cx)->W(kH)->H(w)->Child(vtrack);
    }

    int tid = HashClickId(id.s ? id : StrL("slider"));
    // w_full(): the parts are placed by `left(relative(..))` the way Rust's
    // are, so the row the slider sits in decides how long it is. A pixel
    // width keeps the arithmetic it always had.
    bool fill = w == kFill;
    El* track = SliderTrack::New(cx, bind, axis)->H(kH)->Click(tid);
    if (fill) {
        track->W(kFill);
    } else {
        track->W(w);
    }
    if (!disabled) {
        track->FocusId(tid);
    }
    El* rail =
        SliderIndicator::New(cx, bind)->Absolute()->Top(mid)->Left(0)->H(kBar);
    rail->Radius(kBar * 0.5f)->Bg(railBg);
    if (fill) {
        rail->Right(0);
    } else {
        rail->W(w);
    }
    track->Child(rail);
    // Reversed, the filled part is what is left beyond the thumb.
    float fillFrom = reverse ? hi : low;
    float fillTo = reverse ? 1.f : hi;
    El* bar2 = Div(a)->Absolute()->Top(mid)->H(kBar);
    bar2->Radius(kBar * 0.5f)->Bg(fillBg);
    if (fill) {
        bar2->Left(0)->LeftRel(fillFrom)->Right(0)->RightRel(1.f - fillTo);
    } else {
        bar2->Left(w * fillFrom)->W(w * (fillTo - fillFrom));
    }
    track->Child(bar2);
    for (int i = 0; i < (range ? 2 : 1); i++) {
        float at = (range && i == 0) ? low : hi;
        El* thumb = SliderThumb::New(cx)
                        ->Absolute()
                        ->Top((kH - kThumb) * 0.5f)
                        ->W(kThumb)
                        ->H(kThumb)
                        ->Radius(kThumb * 0.5f)
                        ->Bg(th.tokens.sliderThumb)
                        ->Border(1, thumbBorder);
        if (fill) {
            thumb->Left(-kThumb * 0.5f)->LeftRel(at);
        } else {
            thumb->Left(w * at - kThumb * 0.5f);
        }
        track->Child(thumb);
    }
    El* root = gpui::Slider::New(cx)->H(kH)->Child(track);
    return fill ? root->W(kFill) : root->W(w);
}

} // namespace component
} // namespace gpui
