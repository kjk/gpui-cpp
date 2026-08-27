#ifndef GPUI_SYS_GPU_H_
#define GPUI_SYS_GPU_H_
/* This process' share of the GPU — crates/fps/src/gpu.rs
 *
 * The HUD reports CPU and memory from sysinfo.h; this is the third reading,
 * and for a renderer it is the half that usually explains a slow frame. It is
 * this process' own share rather than the machine's, which is what Task
 * Manager's GPU column shows for a process.
 *
 * Not every platform publishes one. `GpuAvailable()` says whether this build
 * has a probe at all, and `GpuUsagePercent` answers -1 when there is nothing
 * to read — the HUD leaves the row out rather than showing a zero, the way
 * Rust's `Option<f32>` does.
 */

#ifndef GPUI_SYS_GPU_H_
#define GPUI_SYS_GPU_H_

#include "base.h"

namespace gpui {

// Whether this build can read a GPU figure. False on the platforms whose
// probe is not written (see src/sys/gpu_posix.cpp), which is Rust's
// `gpu/unsupported.rs`.
bool GpuAvailable();

// This process' GPU utilization, 0..100, or -1 when there is nothing to read.
// Sampled rather than averaged: the counter is already a rolling figure.
float GpuUsagePercent();

// Let go of whatever the probe holds open. The HUD is the only caller and
// asks at shutdown, the way it does of the rest of sys/.
void GpuProbeFree();

} // namespace gpui

#endif // GPUI_SYS_GPU_H_
#endif // GPUI_SYS_GPU_H_
