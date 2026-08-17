/* Unstyled selectable text host — crates/base/src/text_selection.rs */

#include "gpui/Gpui.h"

namespace gpui {

struct TextSelection {
    static El* New(Arena* a, Str id, int clickId = 0);
};
} // namespace gpui
