/* Unstyled scrollbar — crates/base/src/scrollbar.rs */

#include "gpui/gpui.h"

namespace gpui {

// The three pieces of arithmetic a scrollbar is. Each is written along one
// axis: Rust branches on `is_vertical` and does the same thing to y or x, so
// the caller passes whichever pair the axis names.
//
// `track` is the bar's length, `container` the visible size, and `content` the
// scrolled size. Offsets here run positive-down, the way El::ScrollY does;
// Rust's run negative because it offsets the content rather than the view.

// The thumb's length. It shrinks with the ratio of what is visible, and stops
// at a floor so a very long document still leaves something to grab.
float ScrollbarThumbSize(float track, float container, float content);

// Where the thumb starts, for a given offset.
float ScrollbarThumbPos(float track, float thumb, float offset, float container,
                        float content);

// A press on the track away from the thumb: the thumb jumps so its centre is
// under the press. Rust caps this at 1 without a floor, since a press below
// the origin cannot happen inside the bar.
float ScrollbarOffsetForTrackPress(float pos, float trackOrigin, float track,
                                   float thumb, float container, float content);

// A drag. `grab` is how far into the thumb the press landed, so the thumb
// stays under the same point of the pointer rather than snapping its centre
// there — which is the whole difference between this and the press above.
float ScrollbarOffsetForDrag(float pos, float grab, float trackOrigin,
                             float track, float thumb, float container,
                             float content);

// ScrollbarAxis: which bars a scrolled box shows, and which ways the wheel
// moves it. Rust names the axis on the bar — `Scrollbar::vertical(&handle)`,
// `::horizontal`, `::both` — because there the bar is a separate overlay
// element hung off a scrolled div. Here the bar belongs to the box that
// scrolls, the way it does in the renderer under this tree, so the axis is
// asked of the box.
enum class ScrollAxis : uint8_t {
    Vertical,
    Horizontal,
    Both
};

struct Scrollbar {
    // The bare box, for a caller that wires the scroll itself.
    static El* New(Ctx* cx);

    // `div().overflow_scroll().track_scroll(&handle).child(Scrollbar::new(&handle))`
    // in one: the box that clips, the offsets it is scrolled to, the id it
    // scrolls under and where it reports a wheel, a track press or a drag of
    // the thumb. Everything a scrolled region needs is here, so a caller
    // cannot get half of it — a box with no `onScroll` takes no wheel at all,
    // and used to leave the page behind it scrolling instead.
    //
    // `id` is the element id the scroll is tracked under, which is what
    // `track_scroll` names. The offsets are the view's: the box reports where
    // it should now be and the caller stores it, rather than the box moving
    // itself.
    static El* New(Ctx* cx, Str id, float scrollY, float scrollX,
                   Listener onScroll, ScrollAxis axis = ScrollAxis::Vertical,
                   ScrollbarMode mode = ScrollbarMode::Always);

    // The vertical case, which is most of them.
    static El* Vertical(Ctx* cx, Str id, float scrollY, Listener onScroll,
                        ScrollbarMode mode = ScrollbarMode::Always);
};
} // namespace gpui
