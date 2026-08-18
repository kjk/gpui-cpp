#include "Story.h"

struct SliderStory {
    // The Color Picker's four channels, and the logarithmic speed slider.
    float hsl[4] = {0.58f, 0.62f, 0.5f, 1.f};
    float speed = 0.5f;
    // The Default and Range sections track their own values too, so the
    // sliders on this page all answer to a click.
    float volume = 0.75f;
    float priceLo = 0.12f;
    float priceHi = 0.45f;
    float storage = 0.5f;

    static El* Render(SliderStory* self, Ctx* cx);
};

// Where a press landed along the track, as 0..1. ClickEvent carries the box
// it hit, so the fraction is the offset within it; a vertical track counts
// up from the bottom.
static float ClickFraction(const ClickEvent* ev, bool vertical) {
    float span = vertical ? ev->elH : ev->elW;
    if (span <= 0) {
        return 0;
    }
    float at =
        vertical ? (ev->elY + span - ev->y) / span : (ev->x - ev->elX) / span;
    return at < 0 ? 0 : (at > 1 ? 1 : at);
}

static void SetVolume(SliderStory* self, Ctx* cx, const ClickEvent* ev) {
    self->volume = ClickFraction(ev, false);
    Notify(cx);
}
static void SetStorage(SliderStory* self, Ctx* cx, const ClickEvent* ev) {
    self->storage = ClickFraction(ev, false);
    Notify(cx);
}
// A range slider moves whichever end the press is nearer.
static void SetPrice(SliderStory* self, Ctx* cx, const ClickEvent* ev) {
    float at = ClickFraction(ev, false);
    float dLo = at - self->priceLo;
    float dHi = at - self->priceHi;
    if ((dLo < 0 ? -dLo : dLo) <= (dHi < 0 ? -dHi : dHi)) {
        self->priceLo = at;
    } else {
        self->priceHi = at;
    }
    Notify(cx);
}
static void SetHsl(SliderStory* self, Ctx* cx, const ClickEvent* ev,
                   intptr_t ch) {
    if (ch >= 0 && ch < 4) {
        self->hsl[ch] = ClickFraction(ev, true);
    }
    Notify(cx);
}
static void SetSpeed(SliderStory* self, Ctx* cx, const ClickEvent* ev) {
    self->speed = ClickFraction(ev, false);
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
    StorySectionAdd(def,
                    SliderCard(cx, "Output volume",
                               StoryFmt(cx, "%.0f", self->volume * 100.f).s,
                               component::Slider::New(cx, StrL("volume"))
                                   ->Value(self->volume)
                                   ->W(328)
                                   ->OnChange(Listen(cx, &SetVolume))
                                   ->IntoEl(),
                               false));
    page->Child(def);

    El* range = StorySection(cx, "Range",
                             "Choose minimum and maximum values together.");
    StorySectionAdd(
        range, SliderCard(cx, "Price range",
                          StoryFmt(cx, "$%.0f..%.0f", self->priceLo * 100.f,
                                   self->priceHi * 100.f)
                              .s,
                          component::Slider::New(cx, StrL("price"))
                              ->Range(self->priceLo, self->priceHi)
                              ->W(328)
                              ->OnChange(Listen(cx, &SetPrice))
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
        StoryTxt(cx, StoryFmt(cx, "%.0f GB", (1.f - self->storage) * 10.f), 14,
                 th.mutedFg));
    revCard->Child(revHead);
    revCard->Child(component::Slider::New(cx, StrL("storage"))
                       ->Value(self->storage)
                       ->Reverse()
                       ->W(360)
                       ->OnChange(Listen(cx, &SetStorage))
                       ->IntoEl());
    StorySectionAdd(rev, revCard);
    page->Child(rev);

    // Color Picker: four vertical channels, with the color they make in the
    // section's sub-title beside a Clipboard copy.
    Rgba picked =
        RgbaHsla(self->hsl[0], self->hsl[1], self->hsl[2], self->hsl[3]);
    Str hslText =
        StoryFmt(cx, "hsl(%.0f, %.0f%%, %.0f%%)", self->hsl[0] * 360.f,
                 self->hsl[1] * 100.f, self->hsl[2] * 100.f);
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
        float shown = i == 0 ? self->hsl[i] * 360.f : self->hsl[i] * 100.f;
        El* col = Div(a)->FlexCol()->H(128)->Gap(12)->ItemsCenter();
        col->Child(component::Slider::New(cx, Str(kChannelIds[i]))
                       ->Vertical()
                       ->Value(self->hsl[i])
                       ->W(80)
                       ->OnChange(Listen(cx, &SetHsl, i))
                       ->IntoEl());
        col->Child(StoryTxt(cx, Str(kChannels[i]), 13, th.foreground));
        col->Child(StoryTxt(cx, StoryFmt(cx, "%.0f", shown), 13, th.mutedFg));
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
    // 0.25x .. 4x, geometric, so the midpoint of the track is 1x.
    float speedX = 0.25f * powf(16.f, self->speed);
    speedHead->Child(
        StoryTxt(cx, StoryFmt(cx, "%.2f\xC3\x97", speedX), 14, th.mutedFg));
    speedCard->Child(speedHead);
    speedCard->Child(component::Slider::New(cx, StrL("speed"))
                         ->Value(self->speed)
                         ->W(360)
                         ->OnChange(Listen(cx, &SetSpeed))
                         ->IntoEl());
    StorySectionAdd(playback, speedCard);
    page->Child(playback);
    return page;
}

STORY_PAGE(StorySlider, SliderStory);
