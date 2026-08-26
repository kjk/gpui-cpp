#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

// crates/base/examples/showcase/components/resizable.rs. The page is
// `h_resizable("example-resizable")` with two `resizable_panel()` children:
// the first sized 124 with a range of 116..210, the second taking what is
// left. The handle between them, the drag that moves it and the arithmetic
// that settles the neighbours all belong to the group — this page used to
// carry its own mouse down, drag and up, and its own clamp, because the
// group lived where the base could not reach it. Nothing here reads the sizes
// back, so nothing here holds them: the group keys its own off the id.
El* ShowcaseResizable(ShowcaseApp* app, Ctx* cx) {
    (void)app;
    Arena* a = cx->a;
    El* nav =
        Div(a)->SizeFull()->Pad(8)->FlexCol()->Gap(4)->BorderR(1, ScInk());
    nav->Child(TextEl(a, StrL("PROJECT"))->Font(12)->Fg(ScMutedC()));
    const char* items[] = {"Overview", "Components", "Settings"};
    for (int i = 0; i < 3; i++) {
        nav->Child(Div(a)->W(kFill)->H(26)->PadX(8)->ItemsCenter()->Child(
            TextEl(a, Str(items[i]))->Font(12)->Fg(ScInk())));
    }
    El* main =
        Div(a)
            ->SizeFull()
            ->Pad(8)
            ->FlexCol()
            ->Gap(8)
            ->Bg(ScWhite())
            ->Child(TextEl(a, StrL("Workspace"))->Font(12)->Fg(ScInk()))
            ->Child(TextEl(a, StrL("Drag the divider to resize navigation."))
                        ->Font(12)
                        ->Fg(ScMutedC())
                        ->Wrap()
                        ->MaxW(140));
    // The unstyled layer reads no theme, so the hairline's colour comes from
    // the page, the way every other colour in this showcase does.
    El* group = Resizable::New(cx, StrL("example-resizable"))
                    ->HandleColors(ScInk(), ScInk())
                    ->Panel(nav, 124, 116, 210)
                    ->Grow(main)
                    ->IntoEl();
    return Div(a)->W(288)->H(160)->Border(1, ScInk())->Child(group);
}

SHOWCASE_PAGE(CompResizable, ShowcaseResizable);
