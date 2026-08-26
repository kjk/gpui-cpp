#include "Story.h"

// ToggleDisabled: this page's one Options row.
enum {
    SliderActDisabled = 3600
};

struct SliderStory {
    bool disabled = false;
    StoryToolbarState toolbar;
    // Every slider on the page is a SliderState the window writes to, so the
    // page holds values in their own units and never sees a mouse event.
    // The Color Picker's four channels. Rust runs all four 0..1 at 0.01,
    // seeded 0.38 / 0.5 / 0.5 / 0.5 — a half-transparent green — and reads
    // them back as degrees and percent under the tracks.
    SliderState hsl[4] = {
        SliderStateNew(0, 1, SliderSingle(0.38f), 0.01f),
        SliderStateNew(0, 1, SliderSingle(0.5f), 0.01f),
        SliderStateNew(0, 1, SliderSingle(0.5f), 0.01f),
        SliderStateNew(0, 1, SliderSingle(0.5f), 0.01f),
    };
    // 0.25x .. 4x on a logarithmic scale, so the midpoint of the track is 1x.
    SliderState speed = SliderStateNew(0.25f, 4.f, SliderSingle(1.f), 0.05f,
                                       SliderScale::Logarithmic);
    // -255..255 by fifteens, seeded at 75: the value under the label is a
    // long way from the middle of the track, which is the point of it.
    SliderState volume = SliderStateNew(-255, 255, SliderSingle(75), 15);
    SliderState price = SliderStateNew(0, 100, SliderRange(12, 45), 1);
    SliderState storage = SliderStateNew(0, 10, SliderSingle(5), 1);

    static El* Render(SliderStory* self, Ctx* cx);
};

// SliderEvent::Change. The state already holds the new value — which end of a
// range moved, the step it snapped to, the logarithmic mapping — so the page
// only asks for a repaint.
static void OnSliderChange(SliderStory* self, Ctx* cx, const SliderEvent*) {
    (void)self;
    Notify(cx);
}

static void SliderAct(SliderStory* self, Ctx* cx, const ClickEvent*,
                      intptr_t act) {
    if (act == SliderActDisabled) {
        self->disabled = !self->disabled;
    }
    Notify(cx);
}

// on_copied: window.push_notification("Color copied to clipboard.").
static void OnColorCopied(SliderStory*, Ctx* cx,
                          const component::ClipboardEvent*) {
    StoryPushNotification(cx, StrL("Color copied to clipboard."));
}

// Colorize::to_hex: #RRGGBB, or #RRGGBBAA when the color is not opaque.
static Str ColorHex(Ctx* cx, Rgba c) {
    if (c.a < 255) {
        return StoryFmt(cx, "#%02X%02X%02X%02X", c.r, c.g, c.b, c.a);
    }
    return StoryFmt(cx, "#%02X%02X%02X", c.r, c.g, c.b);
}

// Each section is a 360px card: a label and its reading above the track.
static El* SliderCard(Ctx* cx, const char* label, const char* value, El* slider,
                      bool filled) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow(cx->app);
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
    const Theme& th = ThemeNow(cx->app);
    El* page = Div(a)->FlexCol()->Gap(12)->W(kFill)->ItemsCenter();
    StoryToolbarOpt opts[1] = {{"Disabled", self->disabled, SliderActDisabled}};
    page->Child(
        StoryToolbarOptions(cx, self, opts, 1, Listen(cx, &SliderAct), false));

    El* def = StorySection(cx, "Default",
                           "Adjust a single value within a defined range.");
    StorySectionBody(def)->W(512)->ItemsCenter();
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
    StorySectionBody(range)->W(512)->ItemsCenter();
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
    StorySectionBody(rev)->W(512)->ItemsCenter();
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
    Rgba picked = RgbaHsla(self->hsl[0].value.End(), self->hsl[1].value.End(),
                           self->hsl[2].value.End(), self->hsl[3].value.End());
    Str hslText = ColorHex(cx, picked);
    // section(..).w_128().items_center().justify_around(): the four channel
    // columns are the section's own children, spaced by the justification
    // rather than by a row of their own.
    El* picker = StorySection(cx, "Color Picker", nullptr);
    StorySectionBody(picker)->W(512)->ItemsCenter()->JustifyAround();
    El* sub = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    sub->Child(StoryTxt(cx, hslText, 14, picked));
    sub->Child(component::Clipboard::New(cx, StrL("copy-hsl"))
                   ->Value(hslText)
                   ->OnCopied(Listen(cx, &OnColorCopied))
                   ->IntoEl());
    StorySectionSubTitle(picker, sub);
    static const char* kChannels[4] = {"Hue", "Saturation", "Lightness",
                                       "Alpha"};
    static const char* kChannelIds[4] = {"hsl-h", "hsl-s", "hsl-l", "hsl-a"};
    for (int i = 0; i < 4; i++) {
        // v_flex().h_32().gap_3().items_center().justify_center(), holding the
        // slider and one v_flex().items_center() of the two caption lines.
        El* col =
            Div(a)->FlexCol()->H(128)->Gap(12)->ItemsCenter()->JustifyCenter();
        col->Child(
            component::Slider::New(cx, Str(kChannelIds[i]), &self->hsl[i])
                ->Vertical()
                ->W(80)
                ->OnChange(Listen(cx, &OnSliderChange))
                ->IntoEl());
        El* cap = Div(a)->FlexCol()->ItemsCenter();
        // Neither line names a size, so both read at the base.
        cap->Child(StoryTxt(cx, Str(kChannels[i]), 16, th.foreground));
        // Hue reads in degrees, the other three in percent.
        float shown = self->hsl[i].value.End() * (i == 0 ? 360.f : 100.f);
        cap->Child(StoryTxt(cx, StoryFmt(cx, "%.0f", shown), 16, th.mutedFg));
        col->Child(cap);
        StorySectionAdd(picker, col);
    }
    page->Child(picker);

    // Playback speed: a logarithmic scale, so the track is finer near 1x.
    El* playback = StorySection(
        cx, "Playback speed",
        "Logarithmic scales provide finer control near common values.");
    StorySectionBody(playback)->W(512)->ItemsCenter();
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
