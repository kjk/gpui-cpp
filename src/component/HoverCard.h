/* Themed hover card — crates/ui/src/hover_card.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

struct HoverCard {
    Arena* a = nullptr;
    El* trigger = nullptr;
    El* content = nullptr;
    bool open = false;

    static HoverCard* New(Arena* a);
    HoverCard* Trigger(El* e);
    HoverCard* Content(El* e);
    HoverCard* Open(bool v);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
