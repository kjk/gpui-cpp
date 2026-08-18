#include "Story.h"

struct ColorPickerStory {
    uint32_t colorHex = 0x2563eb;
    bool colorOpen = false;
    static El* Render(ColorPickerStory* self, Ctx* cx);
    static void Click(ColorPickerStory* self, Ctx* cx, int id);
};

El* ColorPickerStory::Render(ColorPickerStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(cx, "Theme Color", nullptr);
    StorySectionAdd(sec, component::ColorPicker::New(cx)
                             ->Hex(self->colorHex)
                             ->Open(self->colorOpen)
                             ->IntoEl());
    page->Child(sec);
    return page;
}

void ColorPickerStory::Click(ColorPickerStory* self, Ctx* cx, int id) {
    (void)cx;
    (void)id;
}

STORY_PAGE(StoryColorPicker, ColorPickerStory);
