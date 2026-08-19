/* Themed clipboard — crates/ui/src/clipboard.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

struct Clipboard {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str value = {};
    Listener onCopy;

    static Clipboard* New(Ctx* cx, Str value);
    Clipboard* OnCopy(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
