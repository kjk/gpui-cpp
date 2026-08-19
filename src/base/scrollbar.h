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

struct Scrollbar {
    static El* New(Ctx* cx);
};
} // namespace gpui
