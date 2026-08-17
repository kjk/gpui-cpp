#include "Story.h"

El* SkeletonRender(StoryApp* app, Arena* a) {
    (void)app;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec =
        StorySection(a, "Default", "Placeholder blocks while content loads.");
    El* col = Div(a)->FlexCol()->Gap(8)->W(320);
    col->Child(component::Skeleton::New(a)->W(48)->H(48)->IntoEl());
    col->Child(component::Skeleton::New(a)->W(280)->H(14)->IntoEl());
    col->Child(
        component::Skeleton::New(a)->W(200)->H(14)->Secondary()->IntoEl());
    StorySectionAdd(sec, col);
    page->Child(sec);

    El* card = StorySection(a, "Card", "A typical loading card.");
    El* row = Div(a)->FlexRow()->Gap(12)->W(360);
    row->Child(component::Skeleton::New(a)->W(56)->H(56)->IntoEl());
    El* lines = Div(a)->FlexCol()->Gap(8)->Grow();
    lines->Child(component::Skeleton::New(a)->W(kFill)->H(14)->IntoEl());
    lines->Child(
        component::Skeleton::New(a)->W(180)->H(14)->Secondary()->IntoEl());
    lines->Child(
        component::Skeleton::New(a)->W(140)->H(14)->Secondary()->IntoEl());
    row->Child(lines);
    StorySectionAdd(card, row);
    page->Child(card);
    return page;
}

void SkeletonClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StorySkeleton, SkeletonRender, SkeletonClick);
