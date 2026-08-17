/* Unstyled hover card — crates/base/src/hover_card.rs */

#pragma once

#include "gpui/Gpui.h"

struct HoverCard {
    El* root = nullptr;

    static HoverCard* New(Arena* a, Str id);
    HoverCard* Trigger(El* trigger);
    HoverCard* Content(El* content);
    El* IntoEl();
};
