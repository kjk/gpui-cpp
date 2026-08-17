/* Themed separator — crates/ui/src/separator.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

enum class SeparatorStyle : uint8_t {
    Solid,
    Dashed
};

struct Separator {
    Arena* a = nullptr;
    bool vertical = false;
    SeparatorStyle line = SeparatorStyle::Solid;
    Str label = {};
    Rgba color = {};
    bool hasColor = false;

    static Separator* Vertical(Arena* a);
    static Separator* Horizontal(Arena* a);
    Separator* Dashed();
    Separator* Label(Str s);
    Separator* Color(Rgba c);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
