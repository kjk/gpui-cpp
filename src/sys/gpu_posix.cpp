/* No GPU probe — crates/fps/src/gpu/unsupported.rs
 *
 * Upstream has three: PDH's `GPU Engine` counters on Windows, IOKit's
 * accelerator statistics on macOS, and the DRM fdinfo a Linux driver
 * publishes. Only the first is ported, because it is the one this tree can be
 * held against a running Task Manager. The other two are a day's work each and
 * an unverifiable reading until somebody runs them: `IOServiceMatching
 * ("IOAccelerator")` and the `Device Utilization %` in its performance
 * statistics on macOS, and the fdinfo entries under `/proc/self/fdinfo`
 * filtered to the DRM fds, with `drm-engine-*` nanoseconds differenced against
 * the wall clock, on Linux.
 *
 * Until then the HUD leaves the row out, which is what Rust's `None` does.
 */

#include "sys/gpu.h"

namespace gpui {

bool GpuAvailable() {
    return false;
}

float GpuUsagePercent() {
    return -1.f;
}

void GpuProbeFree() {}

} // namespace gpui
