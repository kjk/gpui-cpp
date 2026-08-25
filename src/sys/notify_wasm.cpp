/* The browser's Notification API: not implemented.

   It exists — and it is the one platform here that would need a permission
   prompt driven by a user gesture before anything could be posted. Nothing
   in this tree asks for one, so `SysNotifyAvailable` answers false and
   system delivery degrades to the in-app toast. */

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
