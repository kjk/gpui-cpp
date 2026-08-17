#include "gpui.h"

using namespace gpui;

// Port of examples/app_assets: load IconName SVGs from an assets folder
// (rust-embed + AssetSource) and show Inbox + Bot centered in a light window.
struct Example {
    static El* Render(Example*, Ctx* cx) {
        Arena* a = cx->a;
        const Theme& th = ThemeNow();
        // Rust Icon default is size_4 / text size = 16px, two icons, gap_2.
        return Div(a)
            ->FlexCol()
            ->SizeFull()
            ->Gap(8)
            ->ItemsCenter()
            ->JustifyCenter()
            ->Bg(th.background)
            ->Child(IconEl(a, IconName::Inbox, 16))
            ->Child(IconEl(a, IconName::Bot, 16));
    }
};

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    App* app = AppNew();
    ThemeSet(ThemeMode::Light);
    AssetsClear();
    AssetsAddDefaultRoots(StrL("app_assets"));
    return AppRunView(L"App Assets", 800, 600, EntityNew<Example>(app).id, app,
                      AppWinOpts{});
}
