/* Themed icon wrapper — crates/ui/src/icon.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

struct Icon {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    IconName name = IconName::None;
    float size = 16;
    Rgba color = {};
    bool hasColor = false;

    static Icon* New(Ctx* cx, IconName name);
    Icon* Size(float v);
    Icon* Color(Rgba c);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
