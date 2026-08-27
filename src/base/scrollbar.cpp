#include "base/scrollbar.h"
#include "base/theme.h"

namespace gpui {

static El* ApplyScrollbarTheme(Ctx* cx, El* box) {
    const BaseTheme* theme = BaseThemeGlobal(cx->app);
    if (!theme) {
        return box;
    }
    const ScrollbarStyles& styles = theme->scrollbar.styles;
    box->scrollThemeSet = true;
    box->scrollMotion = theme->scrollbar.motion;
    box->scrollTrack = styles.track.hasBackground
                           ? styles.track.background
                           : Background(Rgba{});
    box->scrollTrackActive = styles.trackActive.hasBackground
                                 ? styles.trackActive.background
                                 : box->scrollTrack;
    box->scrollTrackActiveBorder = styles.trackActive.border;
    box->scrollTrackActiveBorderSet = styles.trackActive.hasBorder;
    box->scrollThumb = styles.thumb.hasBackground
                           ? styles.thumb.background
                           : Background(Rgba{});
    box->scrollThumbHover = styles.thumbHover.hasBackground
                                ? styles.thumbHover.background
                                : box->scrollThumb;
    box->scrollThumbRadius =
        styles.thumb.hasRadius ? styles.thumb.radius : 0;
    return box;
}

// Rust's floor on the thumb: below this there is nothing left to aim at.
static const float kMinThumb = 48.f;

static float ClampF(float v, float lo, float hi) {
    if (v < lo) {
        return lo;
    }
    return v > hi ? hi : v;
}

float ScrollbarThumbSize(float track, float container, float content) {
    if (content <= 0 || track <= 0) {
        return 0;
    }
    float thumb = track * (container / content);
    if (thumb < kMinThumb) {
        thumb = kMinThumb;
    }
    return thumb > track ? track : thumb;
}

// The offset a percentage along the track comes to. Rust clamps into
// `safe_range`, which is the whole scrollable distance.
static float OffsetForPct(float pct, float container, float content) {
    float max = content - container;
    if (max <= 0) {
        return 0;
    }
    return ClampF(pct, 0.f, 1.f) * max;
}

float ScrollbarThumbPos(float track, float thumb, float offset, float container,
                        float content) {
    float max = content - container;
    if (max <= 0 || track <= thumb) {
        return 0;
    }
    return ClampF(offset / max, 0.f, 1.f) * (track - thumb);
}

float ScrollbarOffsetForTrackPress(float pos, float trackOrigin, float track,
                                   float thumb, float container,
                                   float content) {
    float span = track - thumb;
    if (span <= 0) {
        return 0;
    }
    // The thumb's centre goes to the press, so half its length comes off.
    float pct = (pos - thumb * 0.5f - trackOrigin) / span;
    return OffsetForPct(pct, container, content);
}

float ScrollbarOffsetForDrag(float pos, float grab, float trackOrigin,
                             float track, float thumb, float container,
                             float content) {
    float span = track - thumb;
    if (span <= 0) {
        return 0;
    }
    float pct = (pos - grab - trackOrigin) / span;
    return OffsetForPct(pct, container, content);
}

El* Scrollbar::New(Ctx* cx) {
    Arena* a = cx->a;
    const BaseTheme* theme = BaseThemeGlobal(cx->app);
    El* box = Div(a);
    if (theme) {
        box->ScrollMode(theme->scrollbar.mode);
    }
    return ApplyScrollbarTheme(cx, box);
}

El* Scrollbar::New(Ctx* cx, Str id, float scrollY, float scrollX,
                   Listener onScroll, ScrollAxis axis) {
    const BaseTheme* theme = BaseThemeGlobal(cx->app);
    ScrollbarMode mode =
        theme ? theme->scrollbar.mode : ScrollbarMode::Scrolling;
    return New(cx, id, scrollY, scrollX, onScroll, axis, mode);
}

El* Scrollbar::New(Ctx* cx, Str id, float scrollY, float scrollX,
                   Listener onScroll, ScrollAxis axis, ScrollbarMode mode) {
    return Apply(cx, Div(cx->a), id, scrollY, scrollX, onScroll, axis, mode);
}

El* Scrollbar::Apply(Ctx* cx, El* element, Str id, float scrollY,
                     float scrollX, Listener onScroll, ScrollAxis axis) {
    const BaseTheme* theme = BaseThemeGlobal(cx->app);
    ScrollbarMode mode =
        theme ? theme->scrollbar.mode : ScrollbarMode::Scrolling;
    return Apply(cx, element, id, scrollY, scrollX, onScroll, axis, mode);
}

El* Scrollbar::Apply(Ctx* cx, El* element, Str id, float scrollY,
                     float scrollX, Listener onScroll, ScrollAxis axis,
                     ScrollbarMode mode) {
    El* box = ApplyScrollbarTheme(cx, element ? element : Div(cx->a))
                  ->ScrollMode(mode);
    // Each axis is asked for on its own: a box that only scrolls down still
    // clips what runs off its side.
    box->ClipY();
    if (axis == ScrollAxis::Vertical || axis == ScrollAxis::Both) {
        box->ScrollY(scrollY);
    }
    if (axis == ScrollAxis::Horizontal || axis == ScrollAxis::Both) {
        box->ScrollX(scrollX);
    } else {
        box->ClipX();
    }
    // `div().id(root_id)` over `div().id((id, "area")).track_scroll(..)`:
    // the port's clip and scroll area are one box, so the name goes on it and
    // the scroll handle's identity is that box's place in the tree. The
    // listener is the other half -- without it the box is only a clip and the
    // wheel falls through to whatever is behind it.
    box->Id(id.s ? id : StrL("scrollbar"))->ScrollFromPath();
    if (onScroll.IsValid()) {
        box->OnScroll(onScroll);
    }
    return box;
}

El* Scrollbar::Vertical(Ctx* cx, Str id, float scrollY, Listener onScroll) {
    return New(cx, id, scrollY, 0, onScroll, ScrollAxis::Vertical);
}

El* Scrollbar::Vertical(Ctx* cx, Str id, float scrollY, Listener onScroll,
                        ScrollbarMode mode) {
    return New(cx, id, scrollY, 0, onScroll, ScrollAxis::Vertical, mode);
}
} // namespace gpui
