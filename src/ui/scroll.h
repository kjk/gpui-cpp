/* Themed scroll — crates/ui/src/scroll */

#include "ui/sizing.h"

namespace gpui {

namespace component {

struct Scrollable {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    El* child = nullptr;
    Str id = {};
    float scrollY = 0;
    float h = 200;
    // Where a scrollbar press or drag reports the offset it worked out. The
    // view owns scrollY, so it is the one that stores it.
    Listener onScroll;

    static Scrollable* New(Ctx* cx);
    static Scrollable* New(Ctx* cx, Str id);
    Scrollable* Child(El* e);
    Scrollable* ScrollY(float v);
    Scrollable* H(float v);
    Scrollable* OnScroll(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
