/* Themed link — crates/ui/src/link.rs */

#pragma once

#include "component/Common.h"

namespace gpui {

namespace component {

struct Link {
    Arena* a = nullptr;
    Str id = {};
    Str href = {};
    Str text = {};
    bool disabled = false;
    Func1<Str> onOpen;

    static Link* New(Arena* a, Str id);
    Link* Href(Str s);
    Link* Text(Str s);
    Link* Disabled(bool v);
    Link* OnOpen(Func1<Str> fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
