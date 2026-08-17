/* Themed label — crates/ui/src/label.rs */

#pragma once

#include "component/Common.h"

namespace gpui {

namespace component {

struct Label {
    Arena* a = nullptr;
    Str text = {};
    Str secondary = {};
    bool masked = false;

    static Label* New(Arena* a, Str text);
    Label* Secondary(Str s);
    Label* Masked(bool v);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
