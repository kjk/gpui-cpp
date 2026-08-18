#include "Story.h"

struct ColorPickerStory {
    uint32_t colorHex = 0x2563eb;
    bool colorOpen = false;
    static El* Render(ColorPickerStory* self, Ctx* cx);
    static void OnKey(ColorPickerStory* self, Ctx* cx, const KeyEvent* ev);
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

// Esc closes what this page has open, like an overlay dismiss.
void ColorPickerStory::OnKey(ColorPickerStory* self, Ctx* cx,
                             const KeyEvent* ev) {
    if (ev->vk != VK_ESCAPE) {
        return;
    }
    self->colorOpen = false;
    Notify(cx);
}

STORY_PAGE_KEYS(StoryColorPicker, ColorPickerStory);
