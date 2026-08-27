/* The macOS notification center: not implemented.

   UNUserNotificationCenter refuses to hand a notification to an application
   that is not a bundle in a location the system trusts — which is what Rust
   documents about its own macOS backend. `SysNotifyAvailable` answers false
   and a post goes nowhere, which is what an unbundled Rust build gets from
   the system center; an `InAppAndSystem` notification keeps its in-app half. */

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
void SysNotifyOnResponse(SysNotifyResponseFn) {}
void SysNotifyShutdown() {}

} // namespace gpui
