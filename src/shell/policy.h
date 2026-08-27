#ifndef GPUI_SHELL_POLICY_H_
#define GPUI_SHELL_POLICY_H_

#include "shell/capability.h"
#include "shell/storage.h"

namespace gpui {

struct Policy;

Policy* PolicyNew();
Policy* PolicyNew(const Capabilities& capabilities);
Policy* PolicyRetain(Policy* policy);
void PolicyRelease(Policy* policy);
const Capabilities& PolicyCapabilities(const Policy* policy);

// Returns a retained handle. Capability grants are frozen once handed out;
// edits replace the default policy and never widen an existing holder.
Policy* PolicyDefault();
void PolicySetDefault(Policy* policy);
void PolicyUpdateDefaultCapabilities(const Capabilities& capabilities);
shell::Storage* PolicyStorage(Policy* policy, bool session);
bool PolicySetStoragePath(Policy* policy, Str path, Str* error = nullptr);
bool ShellSetStoragePath(Str path, Str* error = nullptr);

} // namespace gpui
#endif // GPUI_SHELL_POLICY_H_
