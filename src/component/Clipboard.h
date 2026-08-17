/* Themed clipboard — crates/ui/src/clipboard.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

struct Clipboard {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str value = {};
    Func1<Str> onCopy;

    static Clipboard* New(Ctx* cx, Str value);
    Clipboard* OnCopy(Func1<Str> fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
