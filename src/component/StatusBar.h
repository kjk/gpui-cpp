/* Themed status bar — crates/ui/src/status_bar.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

struct StatusBar {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str left = {};
    Str right = {};

    static StatusBar* New(Ctx* cx);
    StatusBar* Left(Str s);
    StatusBar* Right(Str s);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
