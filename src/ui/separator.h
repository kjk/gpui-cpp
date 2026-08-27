#ifndef GPUI_SRC_UI_SEPARATOR_H_
#define GPUI_SRC_UI_SEPARATOR_H_
/* Themed separator — crates/ui/src/separator.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

enum class SeparatorStyle : uint8_t {
    Solid,
    Dashed
};

struct Separator {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    bool vertical = false;
    SeparatorStyle line = SeparatorStyle::Solid;
    Str label = {};
    Rgba color = {};
    bool hasColor = false;

    static Separator* Vertical(Ctx* cx);
    static Separator* Horizontal(Ctx* cx);
    Separator* Dashed();
    Separator* Label(Str s);
    Separator* Color(Rgba c);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
#endif // GPUI_SRC_UI_SEPARATOR_H_
