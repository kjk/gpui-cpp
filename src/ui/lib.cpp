#include "ui/lib.h"

namespace gpui {
namespace component {

void Init(App* app) {
    if (!app) {
        return;
    }
    ThemeRegistryInit(app);
    UiGlobalStateInit(app);
    BaseInit(app);
    ThemeSyncBase(app);

    DatePickerInitKeys();
    ListInitKeys();
    CommandInitKeys();
    NotificationInitSystem(app);
    PopupMenuInitKeys();
    TableInitKeys();
    SelectInitKeys();
}

} // namespace component
} // namespace gpui
