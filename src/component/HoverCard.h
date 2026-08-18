/* Themed hover card — crates/ui/src/hover_card.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

// HoverCard::anchor: which corner of the trigger the card hangs off.
enum class HoverCardAnchor : uint8_t {
    BottomLeft,
    BottomCenter,
    BottomRight,
    TopLeft,
    TopCenter,
    TopRight
};

struct HoverCard {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    El* trigger = nullptr;
    El* content = nullptr;
    bool open = false;
    HoverCardAnchor anchor = HoverCardAnchor::BottomLeft;

    static HoverCard* New(Ctx* cx);
    static HoverCard* New(Ctx* cx, Str id);
    HoverCard* Trigger(El* e);
    HoverCard* Content(El* e);
    HoverCard* Open(bool v);
    HoverCard* Anchor(HoverCardAnchor a);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
