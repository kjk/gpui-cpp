#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

// SliderEvent::Change, raised by the window while the track is pressed or
// dragged. The state already holds the new value; the page only has to ask
// for a repaint.
static void OnSlider(ShowcaseApp* app, Ctx* cx, const SliderEvent*) {
    (void)app;
    Notify(cx);
}

El* ShowcaseSlider(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    float p = app->slider.pctHi;
    if (p < 0) {
        p = 0;
    }
    if (p > 1) {
        p = 1;
    }
    float trackW = 224;
    float thumb = 14;
    float fillW = trackW * p;
    float thumbX = fillW - thumb / 2;
    if (thumbX < 0) {
        thumbX = 0;
    }
    if (thumbX > trackW - thumb) {
        thumbX = trackW - thumb;
    }

    // The track is what a press lands on, so the state is bound here and the
    // window moves it — SliderTrack::on_mouse_down and its on_drag_move.
    El* track = SliderTrack::New(cx, &app->slider)
                    ->Click(HashClickId(StrL("example-slider-track")))
                    ->W(trackW)
                    ->H(28);
    track->Child(SliderIndicator::New(cx, &app->slider)
                     ->Absolute()
                     ->Top(13)
                     ->Left(0)
                     ->W(trackW)
                     ->H(2)
                     ->Bg(Rgb(0xd4, 0xd4, 0xd4)));
    track->Child(Div(a)->Absolute()->Top(13)->Left(0)->W(fillW)->H(2)->Bg(
        Rgb(0x17, 0x17, 0x17)));
    track->Child(SliderThumb::New(cx)
                     ->Absolute()
                     ->Top(7)
                     ->Left(thumbX)
                     ->W(thumb)
                     ->H(thumb)
                     ->Bg(Rgb(0xff, 0xff, 0xff))
                     ->Border(1, Rgb(0x17, 0x17, 0x17)));

    app->slider.onChange = Listen(cx, &OnSlider);

    return Div(a)
        ->FlexCol()
        ->W(trackW)
        ->Child(Div(a)
                    ->FlexRow()
                    ->JustifyBetween()
                    ->W(kFill)
                    ->PadB(8)
                    ->Child(TextEl(a, StrL("Volume"))
                                ->Font(12)
                                ->Fg(Rgb(0x17, 0x17, 0x17)))
                    ->Child(TextEl(a, StrL("Drag to adjust"))
                                ->Font(12)
                                ->Fg(Rgb(0x17, 0x17, 0x17))))
        ->Child(Slider::New(cx)->W(trackW)->H(28)->Child(track));
}

SHOWCASE_PAGE(CompSlider, ShowcaseSlider);
