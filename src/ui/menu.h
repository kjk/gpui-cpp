/* Themed menu — crates/ui/src/menu */

#include "ui/sizing.h"

namespace gpui {

namespace component {

struct Menu {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str items[8] = {};
    int n = 0;
    Listener onClick;

    static Menu* New(Ctx* cx);
    Menu* Item(Str s);
    Menu* OnClick(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
