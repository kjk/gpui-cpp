#ifndef GPUI_SHELL_ACTION_H_
#define GPUI_SHELL_ACTION_H_
/* The one action a script can name — crates/shell/src/action.rs.

   GPUI actions are Rust types, and a script cannot produce one, so upstream
   collapses the whole family into `ShellAction`, a single type carrying the
   script's own id. Dispatch is then by `TypeId` and the id decides which
   handler runs, which is why upstream's materialize installs one listener per
   element however many actions it registered.

   None of that machinery is needed here. An action in this port is already
   the hash of a name rather than a type (`src/gpui/keymap.h`), so every
   script action is its own action, dispatched and compared exactly the way
   `ui::Cancel` is. What survives from `action.rs` is the naming rule: a
   script id becomes `shell::<id>`, so a script cannot claim `ui::Confirm` and
   two scripts under one runtime cannot be told apart from the host's own. The
   reverse lookup is what stands in for reading the id back off the action a
   listener was handed. */

#include "gpui/keymap.h"

namespace gpui::shell {

// ShellAction::new(id): the action a script's `on_action(id, handler)`,
// `cx.bind_keys` and `window.dispatch_action(id)` all agree on. The name is
// interned, bounded by the number of distinct ids a run ever produces.
uint32_t ShellActionOf(Str id);

// `script_id(action)`: the script's own name for one, or an empty Str for an
// action that is not a script's. What a handler is handed, since upstream
// hands over the id even though the listener was registered for one action.
Str ShellActionScriptId(uint32_t action);

// A `KeyBinding` holds `const char*`, and the keymap keeps the binding for as
// long as the process runs, so a chord parsed out of a script string has to
// outlive the call that produced it. Interned for the same reason and with
// the same bound: a reload rebinding the same chords adds nothing.
const char* ShellActionInternText(Str value);

} // namespace gpui::shell

#endif // GPUI_SHELL_ACTION_H_
