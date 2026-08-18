/* Themed title bar — crates/ui/src/title_bar.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

// TITLE_BAR_HEIGHT.
constexpr float kTitleBarHeight = 34.f;
// TITLE_BAR_LEFT_PADDING: macOS starts after the traffic lights, the other
// two start at the theme's own gutter.
constexpr float kTitleBarLeftPad = GPUI_OS_MAC ? 80.f : 12.f;

// A client-drawn title bar for a window opened with WinOpts::clientTitleBar.
// Children are laid out justify-between across the bar; on Windows and Linux
// the minimize / maximize / close controls follow them, and the surface that
// no child claimed is the window's drag region.
struct TitleBar {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    El* bar = nullptr;
    El* content = nullptr;

    static TitleBar* New(Ctx* cx);
    TitleBar* Child(El* e);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
