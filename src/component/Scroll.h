/* Themed scroll — crates/ui/src/scroll */

#include "component/Common.h"

namespace gpui {

namespace component {

struct Scrollable {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    El* child = nullptr;
    float scrollY = 0;
    float h = 200;

    static Scrollable* New(Ctx* cx);
    Scrollable* Child(El* e);
    Scrollable* ScrollY(float v);
    Scrollable* H(float v);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
