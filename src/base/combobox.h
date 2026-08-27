#ifndef GPUI_BASE_COMBOBOX_H_
#define GPUI_BASE_COMBOBOX_H_
/* Unstyled combobox — crates/base/src/combobox.rs */

#include "gpui/gpui.h"
#include "base/select.h"

namespace gpui {

// A combobox *is* a select in Rust: `Combobox::render` builds a
// `Select::new(id)` and forwards open, disabled, both focus handles and all
// three handlers to it, changing only the key context the bindings hang off.
// Both contexts bind the same four keys to the same four actions, so
// SelectActionForKey answers for a combobox too, and this is the same element
// under the name its module gives it.
struct Combobox {
    static El* New(Ctx* cx, Str id);
};
} // namespace gpui
#endif // GPUI_BASE_COMBOBOX_H_
