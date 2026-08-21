/* AutoScroll — crates/base/src/auto_scroll.rs

   The arithmetic behind a drag that has reached the edge of a scrolling box.
   The struct and the constants are declared beside InputState in gpui.h,
   which is what holds one; this is compute_delta, the only part of the Rust
   file that is not the background task driving it. */

#include "gpui/gpui.h"

namespace gpui {

bool AutoScrollComputeDelta(float y, Bounds bounds, float* out) {
    if (!out) {
        return false;
    }
    float bottomTrigger = bounds.Bottom() - kAutoScrollInnerZone;
    float topTrigger = bounds.y + kAutoScrollInnerZone;
    float ramp = kAutoScrollInnerZone + kAutoScrollOuterRamp;
    float span = kAutoScrollMaxSpeed - kAutoScrollMinSpeed;
    if (y > bottomTrigger) {
        float t = (y - bottomTrigger) / ramp;
        if (t > 1.f) {
            t = 1.f;
        }
        *out = kAutoScrollMinSpeed + t * span;
        return true;
    }
    if (y < topTrigger) {
        float t = (topTrigger - y) / ramp;
        if (t > 1.f) {
            t = 1.f;
        }
        *out = -(kAutoScrollMinSpeed + t * span);
        return true;
    }
    // The dead zone between the two triggers, where a drag scrolls nothing.
    return false;
}

} // namespace gpui
