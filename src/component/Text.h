/* Themed text / markdown view — crates/ui/src/text */

#include "component/Common.h"

namespace gpui {

namespace component {

struct TextView {
    Arena* a = nullptr;
    Str source = {};

    static TextView* New(Arena* a, Str source);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
