#include "base/theme.h"

namespace gpui {

BaseTheme* BaseThemeGlobal(App* app) {
    return AppGlobalEnsure<BaseTheme>(app);
}

const BaseTheme* BaseThemeGlobal(const App* app) {
    return AppGlobalGet<BaseTheme>(app);
}

void BaseThemeSet(App* app, const BaseTheme& theme) {
    BaseTheme* current = BaseThemeGlobal(app);
    if (current) {
        *current = theme;
    }
}

} // namespace gpui
