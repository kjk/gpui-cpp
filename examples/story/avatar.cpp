#include "Story.h"

// AVATARS in avatar_story.rs, verbatim. gpui/image.h fetches an https URL
// with sys/http.h, so these are the same eleven pictures the Rust window
// shows; an unreachable one leaves the avatar empty, as `img()` does.
static const char* kAvatars[] = {
    "https://avatars.githubusercontent.com/u/5518?v=4",
    "https://avatars.githubusercontent.com/u/28998859?v=4",
    "https://avatars.githubusercontent.com/u/20092316?v=4",
    "https://avatars.githubusercontent.com/u/22312482?v=4",
    "https://avatars.githubusercontent.com/u/150917089?v=4",
    "https://avatars.githubusercontent.com/u/20337280?v=4",
    "https://avatars.githubusercontent.com/u/629429?v=4",
    "https://avatars.githubusercontent.com/u/583231?v=4",
    "https://avatars.githubusercontent.com/u/1264109?v=4",
    "https://avatars.githubusercontent.com/u/2936367?v=4",
    "https://avatars.githubusercontent.com/u/1253486?v=4",
};
static const int kAvatarCount = (int)(sizeof(kAvatars) / sizeof(kAvatars[0]));

struct AvatarStory {
    StoryToolbarState toolbar;

    static El* Render(AvatarStory* self, Ctx* cx);
};

static component::Avatar* Face(Ctx* cx, int ix) {
    return component::Avatar::New(cx)->Src(Str(kAvatars[ix]));
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
    StorySectionAdd(
        img, Face(cx, 0)->Name(StrL("Jason Lee"))->WithSize(size)->IntoEl());
    StorySectionAdd(img, Face(cx, 1)->WithSize(size)->IntoEl());
    page->Child(img);

    El* fb = StorySection(
        cx, "Fallback", "Show initials or an icon when no image is available.");
    StorySectionBody(fb)->W(512);
    StorySectionAdd(fb, component::Avatar::New(cx)
                            ->Name(StrL("Jason Lee"))
                            ->WithSize(size)
                            ->IntoEl());
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
        g1->Child(Face(cx, i));
    }
    StorySectionAdd(grp, g1->IntoEl());
    component::AvatarGroup* g2 =
        component::AvatarGroup::New(cx)->WithSize(size)->Limit(5)->Ellipsis();
    for (int i = 0; i < kAvatarCount; i++) {
        g2->Child(Face(cx, i));
    }
    StorySectionAdd(grp, g2->IntoEl());
    page->Child(grp);

    El* shape = StorySection(cx, "Custom shape",
                             "Set an explicit size and corner radius.");
    StorySectionAdd(shape, Face(cx, 0)->Size(100)->Radius(20)->IntoEl());
    page->Child(shape);

    El* style = StorySection(cx, "Custom style",
                             "Add borders and shadows to the image.");
    StorySectionAdd(style,
                    Face(cx, 2)->Size(100)->Border(3, th.foreground)->IntoEl());
    page->Child(style);
    return page;
}

STORY_PAGE(StoryAvatar, AvatarStory);
