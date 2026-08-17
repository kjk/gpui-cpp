#include "Story.h"

El* AvatarRender(StoryApp* app, Arena* a) {
    (void)app;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(
        a, "Fallback", "Show initials or an icon when no image is available.");
    El* row = Div(a)->FlexRow()->Gap(12)->ItemsCenter();
    row->Child(component::Avatar::New(a)
                   ->Initials(StrL("JL"))
                   ->Bg(Rgb(0xf4, 0xd4, 0xd8))
                   ->Size(40)
                   ->IntoEl());
    row->Child(component::Avatar::New(a)
                   ->Initials(StrL(""))
                   ->Bg(Rgb(0xe5, 0xe5, 0xe5))
                   ->Size(40)
                   ->IntoEl());
    row->Child(component::Avatar::New(a)
                   ->Initials(StrL(""))
                   ->Bg(Rgb(0xe5, 0xe5, 0xe5))
                   ->Size(40)
                   ->IntoEl());
    StorySectionAdd(sec, row);
    page->Child(sec);
    return page;
}

void AvatarClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryAvatar, AvatarRender, AvatarClick);
