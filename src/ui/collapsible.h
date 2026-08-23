/* Themed collapsible — crates/ui/src/collapsible.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

struct Collapsible {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    bool open = false;
    El* trigger = nullptr;
    El* content = nullptr;
    // The caller's own style on the collapsible's root: `w_full()` and
    // `gap_2()` are what every one of the story's carries.
    float width = 0;
    float gap = 0;

    static Collapsible* New(Ctx* cx);
    Collapsible* W(float v);
    Collapsible* Gap(float v);
    Collapsible* Open(bool v);
    Collapsible* Trigger(El* e);
    Collapsible* Content(El* e);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
