/* Unstyled resizable — crates/base/src/resizable */

#include "gpui/gpui.h"

namespace gpui {

// PANEL_MIN_SIZE. A panel never shrinks below this unless its own range says
// otherwise.
const float kResizablePanelMinSize = 100.f;

// resize_panel_at_handle: set panel `ix` to `size` and settle the rest.
//
// `ix` names the handle between panel ix and ix + 1, so the last panel has
// none and answers false. Growing takes the space from the panels after it,
// one at a time, each down to its own minimum; shrinking gives it back to the
// panel immediately after and pulls from the ones before if that is not
// enough. A total that still overruns the container comes off the panel that
// was dragged, which is Rust's last correction.
//
// `sizes` is read and written in place. `mins` and `maxs` are the per-panel
// range — pass null for either to use kResizablePanelMinSize and no ceiling.
// Answers false when there was nothing to do, which is Rust's early return.
bool ResizablePanelResize(float* sizes, const float* mins, const float* maxs,
                          int n, int ix, float size, float containerSize);

// adjust_to_container_size: every panel keeps the share it had when the
// container changes size.
void ResizableAdjustToContainer(float* sizes, int n, float containerSize);

struct Resizable {
    static El* New(Ctx* cx, Str id);
};
struct ResizablePanel {
    static El* New(Ctx* cx);
};
} // namespace gpui
