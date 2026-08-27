#ifndef GPUI_SRC_UI_SKELETON_H_
#define GPUI_SRC_UI_SKELETON_H_
/* Themed skeleton — crates/ui/src/skeleton.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

struct Skeleton {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    bool secondary = false;
    float w = kFill;
    float h = 16;

    static Skeleton* New(Ctx* cx);
    Skeleton* Secondary();
    Skeleton* W(float v);
    Skeleton* H(float v);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
#endif // GPUI_SRC_UI_SKELETON_H_
