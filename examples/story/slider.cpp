#include "Story.h"

struct SliderStory {
    static El* Render(SliderStory* self, Ctx* cx);
};

static void SetSlider(SliderStory*, Ctx*, const ClickEvent*) {}

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

El* SliderStory::Render(SliderStory*, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* page = Div(a)->FlexCol()->Gap(12)->W(kFill)->ItemsCenter();

    El* def = StorySection(cx, "Default",
                           "Adjust a single value within a defined range.");
    StorySectionAdd(def, SliderCard(cx, "Output volume", "75",
                                    component::Slider::New(cx)
                                        ->Value(0.75f)
                                        ->W(328)
                                        ->OnChange(Listen(cx, &SetSlider))
                                        ->IntoEl(),
                                    false));
    page->Child(def);

    El* range = StorySection(cx, "Range",
                             "Choose minimum and maximum values together.");
    StorySectionAdd(
        range,
        SliderCard(
            cx, "Price range", "$12..45",
            component::Slider::New(cx)->Range(0.12f, 0.45f)->W(328)->IntoEl(),
            true));
    page->Child(range);

    El* rev = StorySection(
        cx, "Reverse", "Reverse the fill direction for remaining capacity.");
    El* revCard = Div(a)->FlexCol()->W(360)->Gap(16);
    El* revHead = Div(a)->FlexRow()->W(kFill)->ItemsCenter()->JustifyBetween();
    revHead->Child(StoryTxt(cx, StrL("Storage remaining"), 16, th.foreground)
                       ->Semibold());
    revHead->Child(StoryTxt(cx, StrL("5 GB"), 14, th.mutedFg));
    revCard->Child(revHead);
    revCard->Child(
        component::Slider::New(cx)->Value(0.5f)->Reverse()->W(360)->IntoEl());
    StorySectionAdd(rev, revCard);
    page->Child(rev);
    return page;
}

STORY_PAGE(StorySlider, SliderStory);
