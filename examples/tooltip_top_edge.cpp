#include "gpui.h"

using namespace gpui;

// examples/tooltip_top_edge — a trigger pinned to the top edge, so the
// tooltip has to flip below it.
struct Example {
    static El* Render(Example*, Ctx* cx) {
        Arena* a = cx->a;
        const Theme& th = cx->theme();
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

int GpuiMain(int argc, char** argv) {
    (void)argc;
    (void)argv;
    App* app = AppNew();
    ThemeSet(app, ThemeMode::Light);
    return AppRunView(StrL("Tooltip Top Edge"), 520, 260,
                      EntityNew<Example>(app).id, app, WinOpts{});
}
