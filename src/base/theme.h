/* Base Theme — crates/base/src/theme.rs

   The themed component palette is still being extracted from gpui::Theme.
   This is the application-owned Base global Rust exposes: semantic tokens
   plus behavior defaults projected by the UI layer. */

#include "base/theme_tokens.h"

namespace gpui {

// ScrollbarTrackStyle / ScrollbarThumbStyle / ScrollbarStyles. Rust keeps
// these paint-only refinements on the Base theme so the behavior layer never
// has to reach upward into gpui-component's palette.
struct ScrollbarTrackStyle {
    Background background = {};
    Rgba border = {};
    float width = 0;
    bool hasBackground = false;
    bool hasBorder = false;
    bool hasWidth = false;
};

struct ScrollbarThumbStyle {
    Background background = {};
    float width = 0;
    float inset = 0;
    float radius = 0;
    float minLength = 0;
    bool hasBackground = false;
    bool hasWidth = false;
    bool hasInset = false;
    bool hasRadius = false;
    bool hasMinLength = false;
};

struct ScrollbarStyles {
    ScrollbarTrackStyle track = {};
    ScrollbarTrackStyle trackHover = {};
    ScrollbarTrackStyle trackActive = {};
    ScrollbarThumbStyle thumb = {};
    ScrollbarThumbStyle thumbHover = {};
    ScrollbarThumbStyle thumbActive = {};
};

struct BaseScrollbarTheme {
    ScrollbarMode mode = ScrollbarMode::Scrolling;
    ScrollbarMotion motion = {};
    ScrollbarStyles styles = {};
};

struct BaseResizableTheme {
    Rgba handle = {};
    Rgba activeHandle = {};
};

struct BaseTheme {
    SemanticThemeTokens tokens = {};
    BaseScrollbarTheme scrollbar = {};
    BaseResizableTheme resizable = {};
};

BaseTheme* BaseThemeGlobal(App* app);
const BaseTheme* BaseThemeGlobal(const App* app);
void BaseThemeSet(App* app, const BaseTheme& theme);

} // namespace gpui
