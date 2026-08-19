/* Themed kbd — crates/ui/src/kbd.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

struct Kbd {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str stroke = {};
    bool appearance = true;
    bool outline = false;

    static Kbd* New(Ctx* cx, Str stroke);
    Kbd* Appearance(bool v);
    Kbd* Outline();
    El* IntoEl();
};

} // namespace component
} // namespace gpui
