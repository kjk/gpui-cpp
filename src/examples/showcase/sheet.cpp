#include "Showcase.h"
#include "ui/Button.h"
#include "ui/Sheet.h"

enum { ClickSheetOpen = 490, ClickSheetDone = 491 };

El* ShowcaseSheet(ShowcaseApp* app, Arena* a) {
    // Rust is size_full + min_h_64. A wrap-sized page recenters when the
    // overlay mounts and the trigger jumps.
    El* root = Div(a)->FlexCol()->W(kFill)->MinH(256)->ItemsCenter()->JustifyCenter();
    El* trigger = Button::New(a, StrL("open-sheet"), ClickSheetOpen)
                      ->H(28)
                      ->PadX(8)
                      ->ItemsCenter()
                      ->JustifyCenter()
                      ->Border(1, Rgb(0x17, 0x17, 0x17))
                      ->Bg(Rgb(0xff, 0xff, 0xff))
                      ->Child(TextEl(a, StrL("Open settings"))->Font(12)->Fg(Rgb(0x17, 0x17, 0x17)));
    root->Child(trigger);
    if (!app->sheetOpen) {
        return root;
    }
    El* surface = Div(a)
                      ->Absolute()
                      ->Top(0)
                      ->Right(0)
                      ->H(kFill)
                      ->W(210)
                      ->Pad(12)
                      ->FlexCol()
                      ->Bg(ScWhite())
                      ->Border(1, ScInk())
                      ->Child(ScTxt(a, StrL("Settings"), 12, ScInk())->Semibold())
                      ->Child(Div(a)->PadT(16)->W(kFill)->Child(ScTxt(a, StrL("Workspace name"), 12, ScInk())))
                      ->Child(Div(a)->PadT(4)->W(kFill)->Child(Div(a)
                                                                   ->W(kFill)
                                                                   ->H(28)
                                                                   ->PadX(8)
                                                                   ->ItemsCenter()
                                                                   ->Border(1, ScSilver())
                                                                   ->Child(ScTxt(a, StrL("Acme Studio"), 12, ScInk()))))
                      ->Child(Div(a)->PadT(8)->W(kFill)->Child(
                          ScTxt(a, StrL("Update the workspace preferences for your team."), 12, ScGray())->Wrap()))
                      ->Child(Div(a)->PadT(16)->PadY(4)->W(kFill)->BorderT(1, ScBorder())->Child(
                          ScTxt(a, StrL("Notifications  ·  Enabled"), 12, ScInk())))
                      ->Child(Div(a)->PadT(12)->W(kFill)->FlexRow()->JustifyEnd()->Child(
                          Button::New(a, StrL("close-sheet"), ClickSheetDone)
                              ->H(28)
                              ->PadX(12)
                              ->ItemsCenter()
                              ->JustifyCenter()
                              ->Bg(Rgb(0, 0, 0))
                              ->Child(TextEl(a, StrL("Done"))->Font(12)->Fg(Rgb(0xff, 0xff, 0xff)))));

    El* overlay = Div(a)
                      ->Absolute()
                      ->Top(0)
                      ->Left(0)
                      ->W(kFill)
                      ->H(kFill)
                      ->Bg(Rgba8(0, 0, 0, 38))
                      ->Click(ClickSheetDone);
    root->Child(Sheet::New(a)->Overlay(overlay)->Surface(surface)->IntoEl());
    return root;
}

void ShowcaseSheetClick(ShowcaseApp* app, int id) {
    if (id == ClickSheetOpen) {
        app->sheetOpen = true;
    } else if (id == ClickSheetDone) {
        app->sheetOpen = false;
    }
}

SHOWCASE_PAGE(CompSheet, ShowcaseSheet, ShowcaseSheetClick);

