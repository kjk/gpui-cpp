#include "Story.h"

// AVATARS in avatar_story.rs is eleven github URLs. Fetching an https URL
// needs a socket and a TLS stack this tree does not have, so each one is a
// name instead and the avatar falls back to its initials — which is the same
// path Rust takes for the two entries in Fallback.
static const char* kNames[] = {
    "Jason Lee", "huacnlee",  "Tim Wang",     "Alice Brown",
    "Chen Dan",  "Eve Fox",   "Grace Hopper", "Ivan Jones",
    "Kim Lu",    "Mia Novak", "Omar Perez",
};
static const int kNameCount = (int)(sizeof(kNames) / sizeof(kNames[0]));

struct AvatarStory {
    StoryToolbarState toolbar;

    static El* Render(AvatarStory* self, Ctx* cx);
};

static component::Avatar* Face(Ctx* cx, const char* name) {
    return component::Avatar::New(cx)->Name(Str(name));
}

El* AvatarStory::Render(AvatarStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    UiSize size = self->toolbar.size;
    El* page = Div(a)->FlexCol()->Gap(12)->W(kFill);
    page->Child(StoryToolbar(cx, self));

    // Image and Fallback are both .w_128().
    El* img = StorySection(cx, "Image", "Use an image when one is available.");
    StorySectionBody(img)->W(512);
    StorySectionAdd(img, Face(cx, kNames[0])->WithSize(size)->IntoEl());
    StorySectionAdd(img, Face(cx, kNames[1])->WithSize(size)->IntoEl());
    page->Child(img);

    El* fb = StorySection(
        cx, "Fallback", "Show initials or an icon when no image is available.");
    StorySectionBody(fb)->W(512);
    StorySectionAdd(fb, Face(cx, kNames[0])->WithSize(size)->IntoEl());
    StorySectionAdd(fb, component::Avatar::New(cx)->WithSize(size)->IntoEl());
    StorySectionAdd(fb, component::Avatar::New(cx)
                            ->Placeholder(IconName::Building2)
                            ->WithSize(size)
                            ->IntoEl());
    page->Child(fb);

    // Group is .v_flex().w_128().items_center().gap_5().
    El* grp = StorySection(
        cx, "Group", "Groups can limit visible avatars and show overflow.");
    StorySectionBody(grp)->FlexCol()->W(512)->ItemsCenter()->Gap(20);
    // No limit: AvatarGroup's own default of 3 still applies, so six
    // avatars show three.
    component::AvatarGroup* g1 = component::AvatarGroup::New(cx)
                                     ->WithSize(size);
    for (int i = 0; i < 6; i++) {
        g1->Child(Face(cx, kNames[i]));
    }
    StorySectionAdd(grp, g1->IntoEl());
    component::AvatarGroup* g2 =
        component::AvatarGroup::New(cx)->WithSize(size)->Limit(5)->Ellipsis();
    for (int i = 0; i < kNameCount; i++) {
        g2->Child(Face(cx, kNames[i]));
    }
    StorySectionAdd(grp, g2->IntoEl());
    page->Child(grp);

    El* shape = StorySection(cx, "Custom shape",
                             "Set an explicit size and corner radius.");
    StorySectionAdd(shape,
                    Face(cx, kNames[0])->Size(100)->Radius(20)->IntoEl());
    page->Child(shape);

    El* style = StorySection(cx, "Custom style",
                             "Add borders and shadows to the image.");
    StorySectionAdd(
        style,
        Face(cx, kNames[2])->Size(100)->Border(3, th.foreground)->IntoEl());
    page->Child(style);
    return page;
}

STORY_PAGE(StoryAvatar, AvatarStory);
