/* Themed collapsible — crates/ui/src/collapsible.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

struct Collapsible {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    bool open = false;
    El* trigger = nullptr;
    El* content = nullptr;

    static Collapsible* New(Ctx* cx);
    Collapsible* Open(bool v);
    Collapsible* Trigger(El* e);
    Collapsible* Content(El* e);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
