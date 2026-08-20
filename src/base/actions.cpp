#include "base/actions.h"
#include "gpui/keymap.h"

namespace gpui {

namespace action {

// The hash is cheap, but it is the same hash every frame for the life of the
// process, so each name is taken once and kept.
#define GPUI_ACTION(fn, name)                                                  \
    uint32_t fn() {                                                            \
        static uint32_t id = 0;                                                \
        if (!id) {                                                             \
            id = ActionOf(StrL(name));                                         \
        }                                                                      \
        return id;                                                             \
    }

GPUI_ACTION(Confirm, "ui::Confirm")
GPUI_ACTION(ConfirmSecondary, "ui::ConfirmSecondary")
GPUI_ACTION(Cancel, "ui::Cancel")
GPUI_ACTION(SelectUp, "ui::SelectUp")
GPUI_ACTION(SelectDown, "ui::SelectDown")
GPUI_ACTION(SelectLeft, "ui::SelectLeft")
GPUI_ACTION(SelectRight, "ui::SelectRight")
GPUI_ACTION(SelectFirst, "ui::SelectFirst")
GPUI_ACTION(SelectLast, "ui::SelectLast")
GPUI_ACTION(SelectPrevColumn, "ui::SelectPrevColumn")
GPUI_ACTION(SelectNextColumn, "ui::SelectNextColumn")
GPUI_ACTION(SelectPageUp, "ui::SelectPageUp")
GPUI_ACTION(SelectPageDown, "ui::SelectPageDown")

#undef GPUI_ACTION

} // namespace action

} // namespace gpui
