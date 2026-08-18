#include "Story.h"

struct SkeletonStory {
    static El* Render(SkeletonStory* self, Ctx* cx);
};

El* SkeletonStory::Render(SkeletonStory*, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* text = StorySection(
        cx, "Text",
        "Represents an avatar and text while profile content loads.");
    El* textRow = Div(a)->FlexRow()->Gap(12)->W(360)->ItemsCenter();
    textRow->Child(
        component::Skeleton::New(cx)->W(48)->H(48)->IntoEl()->Radius(24));
    El* lines = Div(a)->FlexCol()->Gap(8)->Grow();
    lines->Child(component::Skeleton::New(cx)->W(kFill)->H(16)->IntoEl());
    lines->Child(component::Skeleton::New(cx)->W(kFill)->H(16)->IntoEl()->WFrac(
        2.f / 3.f));
    textRow->Child(lines);
    StorySectionAdd(text, textRow);
    page->Child(text);

    El* card = StorySection(
        cx, "Card", "Combines media and text placeholders in a content card.");
    El* cardCol = Div(a)->FlexCol()->Gap(8)->W(360);
    cardCol->Child(component::Skeleton::New(cx)->W(kFill)->H(180)->IntoEl());
    cardCol->Child(component::Skeleton::New(cx)->W(kFill)->H(16)->IntoEl());
    cardCol->Child(component::Skeleton::New(cx)->W(200)->H(16)->IntoEl());
    StorySectionAdd(card, cardCol);
    page->Child(card);
    return page;
}

STORY_PAGE(StorySkeleton, SkeletonStory);
