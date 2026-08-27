#ifndef GPUI_SHELL_MATERIALIZE_H_
#define GPUI_SHELL_MATERIALIZE_H_

#include "shell/runtime.h"

namespace gpui {

// Replays one immutable script snapshot into the ordinary native element
// tree. This function does not enter QuickJS; callbacks only carry ids back
// to ScriptView for a later event dispatch.
El* ShellMaterialize(Ctx* cx, ShellRuntime* runtime,
                     const RenderSnapshot* snapshot,
                     ShellError* error = nullptr);
El* ShellMaterializeSpec(Ctx* cx, ShellRuntime* runtime,
                         const shell::SpecArena* specs, shell::SpecId root,
                         ShellError* error = nullptr);

} // namespace gpui
#endif // GPUI_SHELL_MATERIALIZE_H_
