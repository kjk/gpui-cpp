#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

enum {
    ClickSwitch = 510
};

El* ShowcaseSwitch(ShowcaseApp* app, Arena* a) {
    bool on = app->switchOn;
    El* track = SwitchTrack::New(a, StrL("example-switch-track"))
                    ->W(36)
                    ->H(20)
                    ->Pad(2)
                    ->Bg(on ? Rgb(0x17, 0x17, 0x17) : Rgb(0xd4, 0xd4, 0xd4))
                    ->ItemsCenter()
                    ->Child(SwitchThumb::New(a)->W(16)->H(16)->Bg(
                        Rgb(0xff, 0xff, 0xff)));
    if (on) {
        track->JustifyEnd();
    } else {
        track->JustifyStart();
    }
    return Div(a)
        ->W(256)
        ->FlexRow()
        ->ItemsCenter()
        ->JustifyBetween()
        ->Child(
            Div(a)
                ->FlexCol()
                ->Child(TextEl(a, StrL("Automatic updates"))
                            ->Font(12)
                            ->Fg(Rgb(0x17, 0x17, 0x17)))
                ->Child(Div(a)->PadT(4)->Child(
                    TextEl(a, StrL("Install stable releases automatically."))
                        ->Font(12)
                        ->Fg(Rgb(0x73, 0x73, 0x73)))))
        ->Child(Switch::New(a, StrL("example-switch"), ClickSwitch)
                    ->Child(track));
}

void ShowcaseSwitchClick(ShowcaseApp* app, int id) {
    if (id == ClickSwitch) {
        app->switchOn = !app->switchOn;
    }
}

SHOWCASE_PAGE(CompSwitch, ShowcaseSwitch, ShowcaseSwitchClick);
