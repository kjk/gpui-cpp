/* The freedesktop notification daemon: not implemented.

   org.freedesktop.Notifications is a D-Bus service, and this tree carries no
   D-Bus client — the X11 half is Xlib, cairo and Pango and nothing else.
   Rust's Linux backend is itself the weakest of the three (it documents that
   a daemon must be running and that retraction is unsupported), and a system
   notification that cannot be retracted is one this component would have to
   describe as best-effort anyway. `SysNotifyAvailable` answers false, so the
   system half is dropped the way it is on a Linux host with no daemon. */

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
