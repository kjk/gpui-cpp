/* The macOS notification center: not implemented.

   UNUserNotificationCenter refuses to hand a notification to an application
   that is not a bundle in a location the system trusts — which is what Rust
   documents about its own macOS backend, where a plain `cargo run` degrades
   to in-app only. That degrading is what this is: `SysNotifyAvailable`
   answers false and a post goes nowhere, so a caller asking for system
   delivery gets what it would have got from an unbundled Rust build. */

#include "sys/notify.h"

namespace gpui {

bool SysNotifyAvailable() {
    return false;
}
void SysNotifySetAppIdentity(Str, Str) {}
bool SysNotifyShow(Str, Str, Str) {
    return false;
}
void SysNotifyDismiss(Str) {}
void SysNotifyOnResponse(SysNotifyResponseFn, void*) {}
void SysNotifyShutdown() {}

} // namespace gpui
