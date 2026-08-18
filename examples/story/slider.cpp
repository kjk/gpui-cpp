#include "Story.h"

struct SliderStory {
    static El* Render(SliderStory* self, Ctx* cx);
    static void Click(SliderStory* self, Ctx* cx, int id);
};

static void SetSlider(SliderStory* self, float v) {
    (void)v;
}

El* SliderStory::Render(SliderStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(
        cx, "Default",
        "An input where the user selects a value from within a given range.");
    El* col = Div(a)->FlexCol()->Gap(8)->W(280);
    col->Child(component::Slider::New(cx)
                   ->Value(0.64f)
                   ->OnChange(MkFunc1(&SetSlider, self))
                   ->IntoEl());
    StorySectionAdd(sec, col);
    page->Child(sec);

    El* range = StorySection(cx, "Range", nullptr);
    El* rangeCol = Div(a)->FlexCol()->Gap(8)->W(280);
    rangeCol->Child(component::Slider::New(cx)->Value(0.25f)->IntoEl());
    rangeCol->Child(component::Slider::New(cx)->Value(0.75f)->IntoEl());
    StorySectionAdd(range, rangeCol);
    page->Child(range);

    El* rev = StorySection(cx, "Reverse", nullptr);
    StorySectionAdd(rev, component::Slider::New(cx)->Value(0.35f)->IntoEl());
    page->Child(rev);
    return page;
}

void SliderStory::Click(SliderStory* self, Ctx* cx, int id) {
    (void)cx;
    (void)id;
}

STORY_PAGE(StorySlider, SliderStory);
