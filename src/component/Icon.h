/* Themed icon wrapper — crates/ui/src/icon.rs */

#pragma once

#include "component/Common.h"

namespace component {

struct Icon {
    Arena* a = nullptr;
    IconName name = IconName::None;
    float size = 16;
    Rgba color = {};
    bool hasColor = false;

    static Icon* New(Arena* a, IconName name);
    Icon* Size(float v);
    Icon* Color(Rgba c);
    El* IntoEl();
};

} // namespace component
