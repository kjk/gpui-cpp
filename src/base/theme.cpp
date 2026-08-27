#include "base/theme.h"

namespace gpui {

base_theme::Theme base_theme::Theme::Global(const App* app) {
    const Theme* installed = AppGlobalGet<Theme>(app);
    return installed ? *installed : Theme{};
}

base_theme::Theme* base_theme::Theme::GlobalMut(App* app) {
    return AppGlobalEnsure<Theme>(app);
}

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
