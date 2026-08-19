/* Unstyled slider — crates/base/src/slider.rs */

#include "gpui/Gpui.h"

namespace gpui {

// gpui::Axis. The slider is the only widget here that takes one so far; it
// moves up to Gpui.h when a second one does.
enum class Axis : uint8_t {
    Horizontal,
    Vertical
};

inline bool AxisIsHorizontal(Axis a) {
    return a == Axis::Horizontal;
}

// SliderValue. Rust's is an enum — `Single(f32)` or `Range(f32, f32)` — and a
// C++ enumerator carries no payload, so it is the pair plus the flag that says
// which variant this is. `hi` is `end()`, the one a single-value slider uses.
struct SliderValue {
    float lo = 0;
    float hi = 0;
    bool range = false;

    float Start() const { return range ? lo : hi; }
    float End() const { return hi; }
};

inline SliderValue SliderSingle(float v) {
    return {0, v, false};
}
inline SliderValue SliderRange(float lo, float hi) {
    return {lo, hi, true};
}
// SliderValue::clamp.
SliderValue SliderValueClamp(SliderValue v, float min, float max);
// SliderValue::set_start / set_end: a range keeps its ends in order.
void SliderValueSetStart(SliderValue* v, float value);
void SliderValueSetEnd(SliderValue* v, float value);

// SliderScale. Logarithmic gives finer control near the low end, which is what
// a volume or a playback speed wants.
enum class SliderScale : uint8_t {
    Linear,
    Logarithmic
};

// SliderState: what a slider is between frames. The view owns one and hands
// the widget a pointer, the way it hands `component::Input` a `LineInput` —
// Rust keeps it in an `Entity<SliderState>`.
struct SliderState {
    float min = 0;
    float max = 100;
    float step = 1;
    SliderValue value = {};
    // percentage: Range<f32>. A single-value slider only uses `hi`, with `lo`
    // pinned at 0, which is what Rust's `0.0..percentage` says.
    float pctLo = 0;
    float pctHi = 0;
    // The box the value maps against, recorded when the indicator paints.
    Bounds bounds = {};
    SliderScale scale = SliderScale::Linear;
    // Set by a press, cleared by the release, so a release with no press
    // behind it emits nothing.
    bool dragging = false;
};

// `.min()` / `.max()`, which re-derive the thumb position. Rust panics when a
// logarithmic slider is given a min <= 0 or a max <= min; there are no
// exceptions here, so the limits are pushed to the nearest usable pair
// instead — a widget that draws itself wrong is better than one that exits.
void SliderSetLimits(SliderState* s, float min, float max);
// `.step()`, the quantum a value snaps to.
void SliderSetStep(SliderState* s, float step);
// `.scale()`.
void SliderSetScale(SliderState* s, SliderScale scale);
// `.default_value()` / `set_value()`.
void SliderSetValue(SliderState* s, SliderValue v);
// set_bounds, from the indicator's own box.
inline void SliderSetBounds(SliderState* s, Bounds b) {
    s->bounds = b;
}

// percentage_to_value / value_to_percentage.
float SliderPctToValue(const SliderState* s, float pct);
float SliderValueToPct(const SliderState* s, float value);
// update_thumb_pos: the percentages that follow from the value.
void SliderUpdateThumbPos(SliderState* s);

// update_value_by_position. `isStart` moves the low end of a range. Rust ends
// this with `cx.emit(SliderEvent::Change)`; there is no emitter here, so it
// returns whether the value moved and the caller raises the event.
bool SliderUpdateByPosition(SliderState* s, Axis axis, Point pos, bool isStart);
// Which end of a range a press at `pos` takes, by the midpoint between the two
// thumbs — the `is_start` arm of SliderTrack's mouse-down handler.
bool SliderIsStartAt(const SliderState* s, Axis axis, Point pos);
// handle_release: true when a Release event is due.
bool SliderHandleRelease(SliderState* s);

struct Slider {
    static El* New(Ctx* cx, int clickId = 0);
};
struct SliderTrack {
    static El* New(Ctx* cx);
};
struct SliderIndicator {
    static El* New(Ctx* cx);
};
struct SliderThumb {
    static El* New(Ctx* cx);
};
} // namespace gpui
