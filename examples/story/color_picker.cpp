#include "Story.h"

El* ColorPickerRender(StoryApp* app, Arena* a) {
    (void)app;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(
        a, "Default", "A color picker that allows users to select a color.");
    StorySectionAdd(sec, component::ColorPicker::New(a)->IntoEl());
    page->Child(sec);
    return page;
}

void ColorPickerClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryColorPicker, ColorPickerRender, ColorPickerClick);
