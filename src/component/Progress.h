/* Themed progress — crates/ui/src/progress */

#pragma once

#include "component/Common.h"

namespace gpui {

namespace component {

struct Progress {
    Arena* a = nullptr;
    float value = 0;
    float w = 200;
    float h = 8;

    static Progress* New(Arena* a);
    Progress* Value(float v);
    Progress* W(float v);
    Progress* H(float v);
    El* IntoEl();
};

struct ProgressCircle {
    Arena* a = nullptr;
    float value = 0;
    float size = 48;

    static ProgressCircle* New(Arena* a);
    ProgressCircle* Value(float v);
    ProgressCircle* Size(float v);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
