#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

static void OpenSheet(ShowcaseApp* app, Ctx* cx, const ClickEvent*) {
    app->sheetOpen = true;
    Notify(cx);
}

static void CloseSheet(ShowcaseApp* app, Ctx* cx, const ClickEvent*) {
    app->sheetOpen = false;
    Notify(cx);
}

El* ShowcaseSheet(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    // Rust is size_full + min_h_64. A wrap-sized page recenters when the
    // overlay mounts and the trigger jumps.
    El* root =
        Div(a)->FlexCol()->W(kFill)->MinH(256)->ItemsCenter()->JustifyCenter();
    El* trigger = Button::New(cx, StrL("open-sheet"))
                      ->OnClick(Listen(cx, &OpenSheet))
                      ->H(28)
                      ->PadX(8)
                      ->ItemsCenter()
                      ->JustifyCenter()
                      ->Border(1, ExampleRgb(0x171717))
                      ->Bg(ExampleRgb(0xffffff))
                      ->Child(TextEl(a, StrL("Open settings"))
                                  ->Font(12)
                                  ->Fg(ExampleRgb(0x171717)));
    root->Child(trigger);
    if (!app->sheetOpen) {
        return root;
    }
    El* surface =
        Div(a)
            ->Absolute()
            ->Top(0)
            ->Right(0)
            ->H(kFill)
            ->W(210)
            ->Pad(12)
            ->FlexCol()
            ->Bg(ScWhite())
            ->Border(1, ScInk())
            ->Child(ScTxt(cx, StrL("Settings"), 12, ScInk())->Semibold())
            ->Child(Div(a)->PadT(16)->W(kFill)->Child(
                ScTxt(cx, StrL("Workspace name"), 12, ScInk())))
            ->Child(Div(a)->PadT(4)->W(kFill)->Child(
                Div(a)
                    ->W(kFill)
                    ->H(28)
                    ->PadX(8)
                    ->ItemsCenter()
                    ->Border(1, ScSilver())
                    ->Child(ScTxt(cx, StrL("Acme Studio"), 12, ScInk()))))
            ->Child(Div(a)->PadT(8)->W(kFill)->Child(
                ScTxt(cx,
                      StrL("Update the workspace preferences for your team."),
                      12, ScGray())
                    ->Wrap()))
            ->Child(Div(a)
                        ->PadT(16)
                        ->PadY(4)
                        ->W(kFill)
                        ->BorderT(1, ScBorder())
                        ->Child(ScTxt(cx, StrL("Notifications  ·  Enabled"), 12,
                                      ScInk())))
            ->Child(Div(a)->PadT(12)->W(kFill)->FlexRow()->JustifyEnd()->Child(
                Button::New(cx, StrL("close-sheet"))
                    ->OnClick(Listen(cx, &CloseSheet))
                    ->H(28)
                    ->PadX(12)
                    ->ItemsCenter()
                    ->JustifyCenter()
                    ->Bg(Rgb(0, 0, 0))
                    ->Child(TextEl(a, StrL("Done"))
                                ->Font(12)
                                ->Fg(ExampleRgb(0xffffff)))));

    El* overlay = Div(a)->Absolute()->Top(0)->Left(0)->W(kFill)->H(kFill)->Bg(
        Rgba8(0, 0, 0, 38));
    root->Child(Sheet::New(cx)
                    ->Overlay(overlay)
                    ->Surface(surface)
                    ->OnClose(Listen(cx, &CloseSheet))
                    ->IntoEl());
    return root;
}

SHOWCASE_PAGE(CompSheet, ShowcaseSheet);
