#include "Story.h"

struct SliderStory {
    // Every slider on the page is a SliderState the window writes to, so the
    // page holds values in their own units and never sees a mouse event.
    // The Color Picker's four channels, in degrees and percent.
    SliderState hsl[4] = {
        SliderStateNew(0, 360, SliderSingle(209)),
        SliderStateNew(0, 100, SliderSingle(62)),
        SliderStateNew(0, 100, SliderSingle(50)),
        SliderStateNew(0, 100, SliderSingle(100)),
    };
    // 0.25x .. 4x on a logarithmic scale, so the midpoint of the track is 1x.
    SliderState speed = SliderStateNew(0.25f, 4.f, SliderSingle(1.f), 0.01f,
                                       SliderScale::Logarithmic);
    SliderState volume = SliderStateNew(0, 100, SliderSingle(75));
    SliderState price = SliderStateNew(0, 100, SliderRange(12, 45));
    SliderState storage = SliderStateNew(0, 10, SliderSingle(5));

    static El* Render(SliderStory* self, Ctx* cx);
};

// SliderEvent::Change. The state already holds the new value — which end of a
// range moved, the step it snapped to, the logarithmic mapping — so the page
// only asks for a repaint.
static void OnSliderChange(SliderStory* self, Ctx* cx, const SliderEvent*) {
    (void)self;
    Notify(cx);
}

// Each section is a 360px card: a label and its reading above the track.
static El* SliderCard(Ctx* cx, const char* label, const char* value, El* slider,
                      bool filled) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* card = Div(a)->FlexCol()->W(360)->Gap(16);
    if (filled) {
        card->Pad(16)->Radius(th.radiusLg)->Bg(RgbaOpacity(th.muted, 0.4f));
    } else {
        card->Pad(16)->Radius(th.radiusLg)->Border(1, th.border);
    }
    El* head = Div(a)->FlexRow()->W(kFill)->ItemsCenter()->JustifyBetween();
    head->Child(StoryTxt(cx, Str(label), 16, th.foreground)->Semibold());
    head->Child(StoryTxt(cx, Str(value), 14, th.mutedFg));
    card->Child(head);
    card->Child(slider);
    return card;
}

El* SliderStory::Render(SliderStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* page = Div(a)->FlexCol()->Gap(12)->W(kFill)->ItemsCenter();

    El* def = StorySection(cx, "Default",
                           "Adjust a single value within a defined range.");
    StorySectionAdd(
        def,
        SliderCard(cx, "Output volume",
                   StoryFmt(cx, "%.0f", self->volume.value.End()).s,
                   component::Slider::New(cx, StrL("volume"), &self->volume)
                       ->W(328)
                       ->OnChange(Listen(cx, &OnSliderChange))
                       ->IntoEl(),
                   false));
    page->Child(def);

    El* range = StorySection(cx, "Range",
                             "Choose minimum and maximum values together.");
    StorySectionAdd(
        range,
        SliderCard(cx, "Price range",
                   StoryFmt(cx, "$%.0f..%.0f", self->price.value.Start(),
                            self->price.value.End())
                       .s,
                   component::Slider::New(cx, StrL("price"), &self->price)
                       ->W(328)
                       ->OnChange(Listen(cx, &OnSliderChange))
                       ->IntoEl(),
                   true));
    page->Child(range);

    El* rev = StorySection(
        cx, "Reverse", "Reverse the fill direction for remaining capacity.");
    El* revCard = Div(a)->FlexCol()->W(360)->Gap(16);
    El* revHead = Div(a)->FlexRow()->W(kFill)->ItemsCenter()->JustifyBetween();
    revHead->Child(StoryTxt(cx, StrL("Storage remaining"), 16, th.foreground)
                       ->Semibold());
    revHead->Child(
        StoryTxt(cx, StoryFmt(cx, "%.0f GB", 10.f - self->storage.value.End()),
                 14, th.mutedFg));
    revCard->Child(revHead);
    revCard->Child(component::Slider::New(cx, StrL("storage"), &self->storage)
                       ->Reverse()
                       ->W(360)
                       ->OnChange(Listen(cx, &OnSliderChange))
                       ->IntoEl());
    StorySectionAdd(rev, revCard);
    page->Child(rev);

    // Color Picker: four vertical channels, with the color they make in the
    // section's sub-title beside a Clipboard copy.
    Rgba picked = RgbaHsla(
        self->hsl[0].value.End() / 360.f, self->hsl[1].value.End() / 100.f,
        self->hsl[2].value.End() / 100.f, self->hsl[3].value.End() / 100.f);
    Str hslText =
        StoryFmt(cx, "hsl(%.0f, %.0f%%, %.0f%%)", self->hsl[0].value.End(),
                 self->hsl[1].value.End(), self->hsl[2].value.End());
    El* picker = StorySection(cx, "Color Picker", nullptr);
    El* sub = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    sub->Child(StoryTxt(cx, hslText, 14, picked));
    sub->Child(component::Clipboard::New(cx, hslText)->IntoEl());
    StorySectionSubTitle(picker, sub);
    static const char* kChannels[4] = {"Hue", "Saturation", "Lightness",
                                       "Alpha"};
    static const char* kChannelIds[4] = {"hsl-h", "hsl-s", "hsl-l", "hsl-a"};
    El* channels = Div(a)->FlexRow()->W(512)->Gap(24)->JustifyCenter();
    for (int i = 0; i < 4; i++) {
        El* col = Div(a)->FlexCol()->H(128)->Gap(12)->ItemsCenter();
        col->Child(
            component::Slider::New(cx, Str(kChannelIds[i]), &self->hsl[i])
                ->Vertical()
                ->W(80)
                ->OnChange(Listen(cx, &OnSliderChange))
                ->IntoEl());
        col->Child(StoryTxt(cx, Str(kChannels[i]), 13, th.foreground));
        col->Child(StoryTxt(cx, StoryFmt(cx, "%.0f", self->hsl[i].value.End()),
                            13, th.mutedFg));
        channels->Child(col);
    }
    StorySectionAdd(picker, channels);
    page->Child(picker);

    // Playback speed: a logarithmic scale, so the track is finer near 1x.
    El* playback = StorySection(
        cx, "Playback speed",
        "Logarithmic scales provide finer control near common values.");
    El* speedCard = Div(a)->FlexCol()->W(360)->Gap(16);
    El* speedHead =
        Div(a)->FlexRow()->W(kFill)->ItemsCenter()->JustifyBetween();
    speedHead->Child(StoryTxt(cx, StrL("Speed"), 16, th.foreground)->Medium());
    speedHead->Child(
        StoryTxt(cx, StoryFmt(cx, "%.2f\xC3\x97", self->speed.value.End()), 14,
                 th.mutedFg));
    speedCard->Child(speedHead);
    speedCard->Child(component::Slider::New(cx, StrL("speed"), &self->speed)
                         ->W(360)
                         ->OnChange(Listen(cx, &OnSliderChange))
                         ->IntoEl());
    StorySectionAdd(playback, speedCard);
    page->Child(playback);
    return page;
}

STORY_PAGE(StorySlider, SliderStory);
