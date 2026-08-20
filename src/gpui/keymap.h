/* Actions and the keymap — crates/gpui's `actions!`, `KeyBinding`,
   `key_context` and `on_action`, which gpui-component builds every component's
   keyboard on.

   Rust dispatches a *type*: an action is a unit struct, a binding names it,
   and `on_action::<Copy>(..)` matches on it. There are no types to dispatch on
   here and no RTTI to ask, so an action is an id — the hash of its name, the
   way an element id is — and a handler is a Listener under that id.

   The rest is Rust's shape exactly. A binding may be scoped to a *key context
   predicate*, an element declares a key context for its subtree, and a
   keystroke is resolved against the contexts stacked along the focused
   element's ancestry, innermost first. The action that comes out is then
   offered to the handlers along that same chain, innermost first, until one
   keeps it; a handler that sets `propagate` passes it on, which is
   `cx.propagate()`. A binding may also be a *sequence* of chords, and a
   keystroke that begins one is held rather than dispatched. */

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

// How deep a stack of key contexts the matcher reads. Deeper than eight
// nested contexts is not a tree anybody builds, and the levels past that are
// the outermost ones — the window's, which a binding scoped to them could
// have gone unscoped instead.
const int kMaxContextDepth = 8;

// The longest sequence a binding may be written as. Rust's Vec has no bound;
// three is past what anything binds — Zed's own keymap stops at two.
const int kMaxStrokes = 3;

// A whole binding spec: the chords of a multi-stroke binding, space
// separated, the way Rust writes `KeyBinding::new("ctrl-k ctrl-o", ..)`.
// Answers how many were read, or 0 for a spec it cannot read.
int KeyChordsParse(Str spec, KeyChord* out, int maxChords);

// An action, which is its name hashed. Rust writes `actions!(root, [Tab])` and
// gets a type; this is the same name, spelled out.
uint32_t ActionOf(Str name);

// KeyContext::parse. A context is a set of identifiers and `key=value` pairs,
// space separated: "Editor", or "Editor mode=full". The id it answers is the
// whole spelling hashed — two elements that declare the same context agree on
// one — and the parse behind it is what a binding's predicate is read
// against. A context is remembered process-wide, since an element rebuilds
// its own every frame out of the same literal.
uint32_t KeyContextOf(Str name);

// `KeyBinding::new(stroke, action, context)`. A null context is Rust's `None`:
// the binding applies wherever focus is. The context is a
// KeyBindingContextPredicate, not just a name:
//
//   "Editor"                  the innermost context declares it
//   "Editor && mode == full"  and carries mode=full
//   "mode != full"            and does not
//   "!Editor"                 does not declare it
//   "Editor || Terminal"      either one
//   "Workspace > Editor"      an Editor whose parent context is a Workspace
//
// `>` binds loosest, then `||`, then `&&`, then `==` / `!=`, then `!`;
// parentheses group. That is Rust's precedence, so "Workspace > Editor &&
// mode == full" reads the way it does there.
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

// KeyMatch. A chord either resolves to an action, or is the first half of a
// binding written as a sequence — `pending`, which means the matcher is
// holding it and the keystroke belongs to nobody else.
struct KeyMatch {
    uint32_t action = 0;
    bool pending = false;
};

// The action a chord resolves to against a stack of contexts, innermost
// first. A binding with no context matches anywhere, but only after every
// scoped binding has been tried: a component's own binding beats the
// application's, which is what makes Escape close the innermost thing.
//
// Stateful, since a sequence is: the chord is appended to whatever is held,
// and a complete binding beats one that is only begun — which is Rust's rule,
// and why "ctrl-k" bound beside "ctrl-k ctrl-o" fires at once. A chord that
// neither completes nor continues anything drops what was held.
KeyMatch KeymapMatch(const KeyChord& chord, const uint32_t* contexts,
                     int nContexts);

// Whether a sequence is half-finished. The window asks before it offers a
// keystroke to the focused field: GPUI matches the keystroke before the text
// input is given it, which is what lets a sequence finish inside one.
bool KeymapPending();
// Drop a half-finished one. A window that loses the focus does this: the rest
// of the sequence is being typed somewhere else, and a chord left waiting
// would take the first keystroke of the window's next visit.
void KeymapClearPending();

} // namespace gpui
