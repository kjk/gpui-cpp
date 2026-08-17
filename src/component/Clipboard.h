/* Themed clipboard — crates/ui/src/clipboard.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

struct Clipboard {
    Arena* a = nullptr;
    Str value = {};
    Func1<Str> onCopy;

    static Clipboard* New(Arena* a, Str value);
    Clipboard* OnCopy(Func1<Str> fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
