#include "Story.h"

struct SkeletonStory {
    static El* Render(SkeletonStory* self, Ctx* cx);
};

El* SkeletonStory::Render(SkeletonStory*, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->W(kFill)->ItemsCenter()->Gap(24);

    El* text = StorySection(
        cx, "Text",
        "Represents an avatar and text while profile content loads.");
    StorySectionBody(text)->W(360);
    El* textRow = Div(a)->FlexRow()->Gap(12)->W(kFill)->ItemsCenter();
    textRow->Child(
        component::Skeleton::New(cx)->W(48)->H(48)->IntoEl()->Radius(24));
    El* lines = Div(a)->FlexCol()->Gap(8)->Flex1();
    lines->Child(component::Skeleton::New(cx)->W(kFill)->H(16)->IntoEl());
    lines->Child(component::Skeleton::New(cx)->W(kFill)->H(16)->IntoEl()->WFrac(
        2.f / 3.f));
    textRow->Child(lines);
    StorySectionAdd(text, textRow);
    page->Child(text);

    El* card = StorySection(
        cx, "Card", "Combines media and text placeholders in a content card.");
    StorySectionBody(card)->W(360);
    // The column has no width of its own in Rust, so it shrink-wraps to its
    // widest child — the 200px line at the bottom — and the two w_full
    // skeletons above follow it.
    El* cardCol = Div(a)->FlexCol()->Gap(8)->W(200);
    cardCol->Child(component::Skeleton::New(cx)->W(kFill)->H(180)->IntoEl());
    cardCol->Child(component::Skeleton::New(cx)->W(kFill)->H(16)->IntoEl());
    cardCol->Child(component::Skeleton::New(cx)->W(200)->H(16)->IntoEl());
    StorySectionAdd(card, cardCol);
    page->Child(card);
    return page;
}

STORY_PAGE(StorySkeleton, SkeletonStory);
