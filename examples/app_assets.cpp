#include "gpui.h"

// Port of examples/app_assets: load IconName SVGs from an assets folder
// (rust-embed + AssetSource) and show Inbox + Bot centered in a light window.

static void OnInit(AppHost* host) {
    (void)host;
    ThemeSet(ThemeMode::Light);
    AssetsClear();
    AssetsAddDefaultRoots(StrL("app_assets"));
}

static El* OnRender(AppHost* host, Arena* frame, WinSize size) {
    (void)host;
    (void)size;
    const Theme& th = ThemeNow();
    // Rust Icon default is size_4 / text size = 16px. Two icons, gap_2 (8px).
    return Div(frame)
        ->FlexCol()
        ->SizeFull()
        ->Gap(8)
        ->ItemsCenter()
        ->JustifyCenter()
        ->Bg(th.background)
        ->Child(IconEl(frame, IconName::Inbox, 16))
        ->Child(IconEl(frame, IconName::Bot, 16));
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    AppHooks hooks = {};
    hooks.onInit = OnInit;
    hooks.onRender = OnRender;
    return RunApp(L"App Assets", 800, 600, hooks, nullptr);
}
