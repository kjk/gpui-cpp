/* No GPU probe — crates/fps/src/gpu/unsupported.rs
 *
 * Upstream has three: PDH's `GPU Engine` counters on Windows, IOKit's
 * accelerator clients on macOS, and the DRM fdinfo a Linux driver publishes.
 * Windows is `gpu_win.cpp`; macOS is `gpu_mac.cpp`. This file is the stub
 * Linux and wasm still compile. `_posix.cpp` is included on macOS too, so
 * the symbols stay compiled out there to avoid colliding with the IOKit
 * probe. The remaining gap is Linux: the fdinfo entries under
 * `/proc/self/fdinfo` filtered to the DRM fds, with `drm-engine-*`
 * nanoseconds differenced against the wall clock.
 *
 * Until then the HUD leaves the row out, which is what Rust's `None` does.
 */

#include "sys/gpu.h"

#if !GPUI_OS_MAC
namespace gpui {

bool GpuAvailable() {
    return false;
}

float GpuUsagePercent() {
    return -1.f;
}

void GpuProbeFree() {}

} // namespace gpui
#endif // !GPUI_OS_MAC
