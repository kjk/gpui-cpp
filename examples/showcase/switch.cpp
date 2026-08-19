#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

// The switch reports the value its activation produces, the way Rust's
// on_change hands the handler `!checked`.
static void OnSwitch(ShowcaseApp* app, Ctx* cx, const ClickEvent*,
                     intptr_t next) {
    app->switchOn = next != 0;
    Notify(cx);
}

El* ShowcaseSwitch(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    bool on = app->switchOn;
    El* track = SwitchTrack::New(cx, StrL("example-switch-track"))
                    ->W(36)
                    ->H(20)
                    ->Pad(2)
                    ->Bg(on ? Rgb(0x17, 0x17, 0x17) : Rgb(0xd4, 0xd4, 0xd4))
                    ->ItemsCenter()
                    ->Child(SwitchThumb::New(cx)->W(16)->H(16)->Bg(
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
        ->Child(Switch::New(cx, StrL("example-switch"), on, false,
                            Listen(cx, &OnSwitch))
                    ->Child(track));
}

SHOWCASE_PAGE(CompSwitch, ShowcaseSwitch);
