/* Actions and the keymap — crates/gpui's `actions!`, `KeyBinding`,
   `key_context` and `on_action`, which gpui-component builds every component's
   keyboard on.

   Rust dispatches a *type*: an action is a unit struct, a binding names it,
   and `on_action::<Copy>(..)` matches on it. There are no types to dispatch on
   here and no RTTI to ask, so an action is an id — the hash of its name, the
   way an element id is — and a handler is a Listener under that id.

   The rest is Rust's shape exactly. A binding may be scoped to a *key
   context*, an element declares one for its subtree, and a keystroke is
   resolved against the contexts stacked along the focused element's ancestry,
   innermost first. The action that comes out is then offered to the handlers
   along that same chain, innermost first, until one keeps it; a handler that
   sets `propagate` passes it on, which is `cx.propagate()`.

   What does not cross: multi-stroke bindings (Rust's "ctrl-k ctrl-o"), and
   predicate contexts (`"Editor && mode == full"`). A context here is a name. */

#include "gpui/gpui.h"

namespace gpui {

// The chord a binding is written as, once parsed. `vk` is the port's own key
// code, which is what a KeyEvent carries.
struct KeyChord {
    int vk = 0;
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
};

// Keystroke::parse. The modifiers Rust spells are `ctrl-`, `alt-`, `shift-`,
// `cmd-` and `secondary-`; the last two are the platform's shortcut key,
// which this port folds onto `ctrl` on every platform — a Cmd-C handler and a
// Ctrl-C handler are the same handler here. The key name is GPUI's:
// lowercase, with "enter", "escape", "tab", "space", "backspace", "delete",
// the four arrows, "home", "end", "pageup", "pagedown", "f1".."f12", a letter
// or a digit, or one of the punctuation keys.
//
// Answers false for a spec it cannot read, which is a programming mistake
// rather than something to handle.
bool KeyChordParse(Str spec, KeyChord* out);
bool KeyChordEq(const KeyChord& a, const KeyChord& b);

// An action, which is its name hashed. Rust writes `actions!(root, [Tab])` and
// gets a type; this is the same name, spelled out.
uint32_t ActionOf(Str name);
// The same for a key context, so a binding and an element agree on one.
uint32_t KeyContextOf(Str name);

// `KeyBinding::new(stroke, action, context)`. A null context is Rust's `None`:
// the binding applies wherever focus is.
struct KeyBinding {
    const char* stroke = nullptr;
    uint32_t action = 0;
    const char* context = nullptr;
};

// cx.bind_keys. The keymap is process-wide, the way the theme and the
// scrollbar mode are: Rust keeps one per App, and an App here is not a
// container for globals. Later bindings win over earlier ones for the same
// chord and context, which is what a keymap layered on top of a default one
// has to do.
void KeymapBind(const KeyBinding* bindings, int n);
void KeymapClear();

// The action a chord resolves to against a stack of contexts, innermost
// first. A binding with no context matches anywhere, but only after every
// scoped binding has been tried: a component's own binding beats the
// application's, which is what makes Escape close the innermost thing.
// Answers 0 for nothing bound.
uint32_t KeymapMatch(const KeyChord& chord, const uint32_t* contexts,
                     int nContexts);

} // namespace gpui
