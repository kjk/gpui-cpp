#include "gpui.h"

using namespace gpui;

// examples/tooltip_top_edge — a trigger pinned to the top edge, so the
// tooltip has to flip below it.
struct Example {
    static El* Render(Example*, Ctx* cx) {
        Arena* a = cx->a;
        const Theme& th = ThemeNow();
        El* btn = ButtonEl(a, 1, StrL("Hover for tooltip"), BtnKind::Primary);
        btn->Tip(StrL(
            "This tooltip should appear below the trigger near the top edge."));

        return Div(a)
            ->SizeFull()
            ->Bg(th.background)
            ->Child(Div(a)->Absolute()->Top(0)->Left(24)->Child(btn))
            ->Child(Div(a)->Absolute()->Top(64)->Left(24)->MaxW(420)->Child(
                TextEl(a,
                       StrL("Hover the top button. The tooltip should flip "
                            "below the trigger without changing the original "
                            "visual gap."))
                    ->Font(14)
                    ->Fg(th.mutedFg)
                    ->Wrap()
                    ->MaxW(420)));
    }
};

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    App* app = AppNew();
    ThemeSet(ThemeMode::Light);
    return AppRunView(L"Tooltip Top Edge", 520, 260, EntityNew<Example>(app).id,
                      app, AppWinOpts{});
}
