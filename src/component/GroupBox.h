/* Themed group box — crates/ui/src/group_box.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

struct GroupBox {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str title = {};
    El* child = nullptr;
    bool outline = false;
    bool filled = true;

    static GroupBox* New(Ctx* cx, Str title);
    GroupBox* Child(El* e);
    GroupBox* Outline();
    GroupBox* Filled(bool v);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
