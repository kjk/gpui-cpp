/* Unstyled accordion — crates/base/src/accordion.rs */

#include "gpui/gpui.h"

namespace gpui {

// The collection root. Identity and nothing else, as in Rust.
struct Accordion {
    static El* New(Ctx* cx, Str id);
};

// Rust's `AccordionTrigger::new(id).open(..).disabled(..).on_change(..)`.
// Activating it asks for the opposite of `open`, which is what `onChange`
// reads as a bool. Like a tab, the trigger takes identity and the click but no
// focus: accordion.rs never calls track_focus.
struct AccordionTrigger {
    static El* New(Ctx* cx, Str id, bool open = false, bool disabled = false,
                   Listener onChange = {});
};

// Rust's heading around the trigger. Its `id` is optional and only there to
// carry the heading role and level, neither of which we have a surface for.
struct AccordionHeader {
    static El* New(Ctx* cx, El* trigger);
};

struct AccordionPanel {
    static El* New(Ctx* cx);
};

// The item pushes `open` and `disabled` down the way Rust's does: the header's
// trigger is told both, and the panel is mounted only while open — unless
// `KeepMounted` asked for it to stay, which is Rust's keep_mounted.
struct AccordionItem {
    El* root = nullptr;
    bool open = false;
    bool keepMounted = false;

    static AccordionItem* New(Ctx* cx);
    AccordionItem* Open(bool v);
    AccordionItem* KeepMounted(bool v);
    AccordionItem* Header(El* header);
    AccordionItem* Panel(El* panel);
    El* IntoEl();
};
} // namespace gpui
