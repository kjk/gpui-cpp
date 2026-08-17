#include "Story.h"

El* SkeletonRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    (void)app;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* text = StorySection(
        a, "Text",
        "Represents an avatar and text while profile content loads.");
    El* textRow = Div(a)->FlexRow()->Gap(12)->W(360)->ItemsCenter();
    textRow->Child(
        component::Skeleton::New(a)->W(48)->H(48)->IntoEl()->Radius(24));
    El* lines = Div(a)->FlexCol()->Gap(8)->Grow();
    lines->Child(component::Skeleton::New(a)->W(kFill)->H(16)->IntoEl());
    lines->Child(component::Skeleton::New(a)->W(200)->H(16)->IntoEl());
    textRow->Child(lines);
    StorySectionAdd(text, textRow);
    page->Child(text);

    El* card = StorySection(
        a, "Card", "Combines media and text placeholders in a content card.");
    El* cardCol = Div(a)->FlexCol()->Gap(8)->W(360);
    cardCol->Child(component::Skeleton::New(a)->W(kFill)->H(180)->IntoEl());
    cardCol->Child(component::Skeleton::New(a)->W(kFill)->H(16)->IntoEl());
    cardCol->Child(component::Skeleton::New(a)->W(240)->H(16)->IntoEl());
    StorySectionAdd(card, cardCol);
    page->Child(card);
    return page;
}

void SkeletonClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StorySkeleton, SkeletonRender, SkeletonClick);
