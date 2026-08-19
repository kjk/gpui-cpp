/* Unstyled hover card — crates/base/src/hover_card.rs */

#include "gpui/gpui.h"

namespace gpui {

struct HoverCard {
    El* root = nullptr;

    static HoverCard* New(Ctx* cx, Str id);
    HoverCard* Trigger(El* trigger);
    HoverCard* Content(El* content);
    El* IntoEl();
};
} // namespace gpui
