#include "base/scrollbar.h"

namespace gpui {

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
    return Div(a);
}

El* Scrollbar::New(Ctx* cx, Str id, float scrollY, float scrollX,
                   Listener onScroll, ScrollAxis axis, ScrollbarMode mode) {
    El* box = New(cx)->ScrollMode(mode);
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

El* Scrollbar::Vertical(Ctx* cx, Str id, float scrollY, Listener onScroll,
                        ScrollbarMode mode) {
    return New(cx, id, scrollY, 0, onScroll, ScrollAxis::Vertical, mode);
}
} // namespace gpui
