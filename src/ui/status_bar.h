/* Themed status bar — crates/ui/src/status_bar.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

struct StatusBar {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str left = {};
    Str center = {};
    Str right = {};
    bool hasLeft = false;
    bool hasCenter = false;
    bool hasRight = false;

    static StatusBar* New(Ctx* cx);
    StatusBar* Left(Str s);
    // The center region is the bar's child; where it lands depends on which
    // sides are filled.
    StatusBar* Center(Str s);
    StatusBar* Right(Str s);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
