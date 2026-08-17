/* Themed popover — crates/ui/src/popover.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

struct Popover {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    El* trigger = nullptr;
    El* content = nullptr;
    bool open = false;

    static Popover* New(Ctx* cx);
    Popover* Trigger(El* e);
    Popover* Content(El* e);
    Popover* Open(bool v);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
