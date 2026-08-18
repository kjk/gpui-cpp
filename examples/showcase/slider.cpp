#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

enum {
    ClickSlider = 500
};

El* ShowcaseSlider(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    float p = app->slider;
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

    El* track = SliderTrack::New(cx)->W(trackW)->H(28);
    track->Child(Div(a)->Absolute()->Top(13)->Left(0)->W(trackW)->H(2)->Bg(
        Rgb(0xd4, 0xd4, 0xd4)));
    track->Child(SliderIndicator::New(cx)
                     ->Absolute()
                     ->Top(13)
                     ->Left(0)
                     ->W(fillW)
                     ->H(2)
                     ->Bg(Rgb(0x17, 0x17, 0x17)));
    track->Child(SliderThumb::New(cx)
                     ->Absolute()
                     ->Top(7)
                     ->Left(thumbX)
                     ->W(thumb)
                     ->H(thumb)
                     ->Bg(Rgb(0xff, 0xff, 0xff))
                     ->Border(1, Rgb(0x17, 0x17, 0x17)));

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
        ->Child(Slider::New(cx, ClickSlider)->W(trackW)->H(28)->Child(track));
}

void ShowcaseSliderClick(ShowcaseApp* app, int id) {
    if (id == ClickSlider) {
        app->draggingSlider = true;
    }
}

void ShowcaseSliderDrag(ShowcaseApp* app, Window* host, float x, float y) {
    (void)y;
    if (!app->draggingSlider && !host->mouseDown) {
        return;
    }
    if (!app->draggingSlider) {
        return;
    }
    // track is centered; approximate using last painted hit? use mouse vs
    // window. Host mouse is absolute. We store track by using a simple mapping:
    // content is centered. Use the slider hit rect from last paint.
    for (int i = 0; i < host->paint.hits.len; i++) {
        HitRect h = host->paint.hits[i];
        if (h.id == ClickSlider) {
            float t = (x - h.x) / (h.w > 1 ? h.w : 1);
            if (t < 0) {
                t = 0;
            }
            if (t > 1) {
                t = 1;
            }
            app->slider = t;
            return;
        }
    }
}

SHOWCASE_PAGE(CompSlider, ShowcaseSlider, ShowcaseSliderClick);
