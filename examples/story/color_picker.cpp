#include "Story.h"

El* ColorPickerRender(StoryApp* app, Arena* a) {
    (void)app;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Theme Color", nullptr);
    StorySectionAdd(sec, component::ColorPicker::New(a)
                             ->Hex(app->colorHex)
                             ->Open(app->colorOpen)
                             ->IntoEl());
    page->Child(sec);
    return page;
}

void ColorPickerClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryColorPicker, ColorPickerRender, ColorPickerClick);
