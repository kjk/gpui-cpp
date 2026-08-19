/* Themed hover card — crates/ui/src/hover_card.rs */

#include "ui/sizing.h"

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

// Whether the card of this id is showing. The delayed open and close live in
// gpui_base::HoverCardState, keyed off the id the way Rust keys it off
// use_keyed_state, so a page asks rather than deciding: it needs the answer
// before it builds the content, which is what Rust's `content` closure is for.
bool HoverCardOpen(Ctx* cx, Str id);

struct HoverCard {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    El* trigger = nullptr;
    El* content = nullptr;
    // Set only by Open(); otherwise the state decides.
    bool controlled = false;
    bool open = false;
    int openDelayMs = 600;
    int closeDelayMs = 300;
    HoverCardAnchor anchor = HoverCardAnchor::BottomLeft;

    static HoverCard* New(Ctx* cx);
    static HoverCard* New(Ctx* cx, Str id);
    HoverCard* Trigger(El* e);
    HoverCard* Content(El* e);
    HoverCard* Open(bool v);
    HoverCard* OpenDelay(int ms);
    HoverCard* CloseDelay(int ms);
    HoverCard* Anchor(HoverCardAnchor a);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
