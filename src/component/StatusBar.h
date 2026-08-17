/* Themed status bar — crates/ui/src/status_bar.rs */

#pragma once

#include "component/Common.h"

namespace gpui {

namespace component {

struct StatusBar {
    Arena* a = nullptr;
    Str left = {};
    Str right = {};

    static StatusBar* New(Arena* a);
    StatusBar* Left(Str s);
    StatusBar* Right(Str s);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
