#include "Story.h"

struct AvatarStory {
    StoryToolbarState toolbar;

    static El* Render(AvatarStory* self, Ctx* cx);
    static void Click(AvatarStory* self, Ctx* cx, int id);
};

static El* Face(Ctx* cx, AvatarStory* self, const char* initials) {
    Arena* a = cx->a;
    return component::Avatar::New(cx)
        ->Initials(Str(initials))
        ->WithSize(self->toolbar.size)
        ->IntoEl();
}

static El* FacePx(Ctx* cx, const char* initials, float px, float radius,
                  float borderW, Rgba borderC) {
    Arena* a = cx->a;
    component::Avatar* av =
        component::Avatar::New(cx)->Initials(Str(initials))->Size(px);
    if (radius >= 0) {
        av->Radius(radius);
    }
    if (borderW > 0) {
        av->Border(borderW, borderC);
    }
    return av->IntoEl();
}

static El* AvatarRow(Ctx* cx, AvatarStory* self, const char** names, int n,
                     int limit, bool ellipsis) {
    Arena* a = cx->a;
    float sz = component::AvatarSizePx(self->toolbar.size);
    int shown = n;
    if (limit > 0 && n > limit) {
        shown = limit;
    }
    // AvatarGroup: item_ml = -avatar_size * 0.3, and the ⋯ chip adds ml_1.
    float step = sz * 0.7f;
    int extra = (ellipsis && n > shown) ? 1 : 0;
    float extraLeft = shown * step + 4;
    El* box =
        Div(a)->H(sz)->W(extra ? extraLeft + sz : sz + (shown - 1) * step);
    // flex_row_reverse lays the row right to left, so the leftmost avatar
    // paints last and sits on top.
    if (extra) {
        box->Child(component::Avatar::New(cx)
                       ->Initials(StrL("\xE2\x8B\xAF"))
                       ->Bg(ThemeNow().secondary)
                       ->WithSize(self->toolbar.size)
                       ->IntoEl()
                       ->Absolute()
                       ->Left(extraLeft));
    }
    for (int i = shown - 1; i >= 0; i--) {
        box->Child(component::Avatar::New(cx)
                       ->Initials(Str(names[i]))
                       ->WithSize(self->toolbar.size)
                       ->IntoEl()
                       ->Absolute()
                       ->Left(i * step));
    }
    return box;
}

El* AvatarStory::Render(AvatarStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(cx, &self->toolbar));

    El* img = StorySection(cx, "Image", "Use an image when one is available.");
    El* imgRow = Div(a)->FlexRow()->Gap(12)->ItemsCenter();
    imgRow->Child(Face(cx, self, "JL"));
    imgRow->Child(Face(cx, self, "HU"));
    StorySectionAdd(img, imgRow);
    page->Child(img);

    El* fb = StorySection(
        cx, "Fallback", "Show initials or an icon when no image is available.");
    El* fbRow = Div(a)->FlexRow()->Gap(12)->ItemsCenter();
    fbRow->Child(Face(cx, self, "JL"));
    fbRow->Child(
        component::Avatar::New(cx)->WithSize(self->toolbar.size)->IntoEl());
    fbRow->Child(component::Avatar::New(cx)
                     ->Placeholder(IconName::Building2)
                     ->WithSize(self->toolbar.size)
                     ->IntoEl());
    StorySectionAdd(fb, fbRow);
    page->Child(fb);

    static const char* kGroupA[] = {"JL", "HU", "TW", "AB", "CD", "EF"};
    static const char* kGroupB[] = {"JL", "HU", "TW", "AB", "CD", "EF",
                                    "GH", "IJ", "KL", "MN", "OP"};
    El* grp = StorySection(
        cx, "Group", "Groups can limit visible avatars and show overflow.");
    El* grpCol = Div(a)->FlexCol()->Gap(20)->ItemsCenter();
    grpCol->Child(AvatarRow(cx, self, kGroupA, 6, 3, false));
    grpCol->Child(AvatarRow(cx, self, kGroupB, 11, 5, true));
    StorySectionAdd(grp, grpCol);
    page->Child(grp);

    El* shape = StorySection(cx, "Custom shape",
                             "Set an explicit size and corner radius.");
    StorySectionAdd(shape, FacePx(cx, "JL", 100, 20, 1, ThemeNow().border));
    page->Child(shape);

    El* style = StorySection(cx, "Custom style",
                             "Add borders and shadows to the image.");
    StorySectionAdd(style, FacePx(cx, "TW", 100, -1, 3, ThemeNow().foreground));
    page->Child(style);
    return page;
}

void AvatarStory::Click(AvatarStory* self, Ctx* cx, int id) {
    if (StoryToolbarClick(&self->toolbar, id)) {
        return;
    }
    (void)cx;
    (void)id;
}

STORY_PAGE(StoryAvatar, AvatarStory);
