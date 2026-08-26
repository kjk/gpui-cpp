#include "base/lib.h"

namespace gpui {

void BaseInit(App* app) {
    if (!app) {
        return;
    }
    (void)BaseThemeGlobal(app);
    BaseGlobalStateInit(app);

    DialogInitKeys();
    DatePickerInitKeys();
    SelectInitKeys();
    InputInitKeys();
    TreeInitKeys();

    // The modules whose escape binding is otherwise installed lazily by the
    // first rendered instance. Rust installs these during crate init.
    CancelInitKeys("Popover");
    SheetInitKeys();
    CancelInitKeys("ColorPicker");
}

} // namespace gpui
