#ifndef GPUI_BASE_AUTO_SCROLL_H_
#define GPUI_BASE_AUTO_SCROLL_H_
/* AutoScroll — crates/base/src/auto_scroll.rs */

#include "gpui/gpui.h"

namespace gpui {

// The constants and AutoScroll state currently live beside InputState while
// input is extracted from the runtime. This module owns the public behavior.
bool AutoScrollComputeDelta(float y, Bounds bounds, float* out);

} // namespace gpui
#endif // GPUI_BASE_AUTO_SCROLL_H_
