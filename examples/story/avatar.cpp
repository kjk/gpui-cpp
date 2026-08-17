#include "Story.h"

El* AvatarRender(StoryApp* app, Arena* a) {
    (void)app;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Fallback", "Initials when an image is not available.");
    El* row = Div(a)->FlexRow()->Gap(12)->ItemsCenter();
    row->Child(component::Avatar::New(a)->Initials(StrL("CN"))->Bg(Rgb(0x17, 0x17, 0x17))->Size(40)->IntoEl());
    row->Child(component::Avatar::New(a)->Initials(StrL("LR"))->Bg(Rgb(0x25, 0x63, 0xeb))->Size(40)->IntoEl());
    row->Child(component::Avatar::New(a)->Initials(StrL("GP"))->Bg(Rgb(0x16, 0xa3, 0x4a))->Size(56)->IntoEl());
    StorySectionAdd(sec, row);
    page->Child(sec);
    return page;
}

void AvatarClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryAvatar, AvatarRender, AvatarClick);
