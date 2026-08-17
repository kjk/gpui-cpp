/* Themed badge — crates/ui/src/badge.rs */

#pragma once

#include "component/Common.h"

namespace component {

enum class BadgeKind : u8 {
    Number,
    Dot,
    Icon
};

struct Badge {
    Arena* a = nullptr;
    int count = 0;
    int max = 99;
    BadgeKind kind = BadgeKind::Number;
    IconName icon = IconName::None;
    Rgba color = {};
    bool hasColor = false;
    UiSize size = UiSize::Medium;
    El* child = nullptr;

    static Badge* New(Arena* a);
    Badge* Count(int n);
    Badge* Max(int n);
    Badge* Dot();
    Badge* Icon(IconName n);
    Badge* Color(Rgba c);
    Badge* WithSize(UiSize s);
    Badge* Child(El* c);
    El* IntoEl();
};

} // namespace component
