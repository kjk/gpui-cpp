#include "Story.h"

static El* Face(Arena* a, StoryApp* app, const char* initials) {
    return component::Avatar::New(a)
        ->Initials(Str(initials))
        ->WithSize(app->size)
        ->IntoEl();
}

static El* FacePx(Arena* a, const char* initials, float px, float radius,
                  float borderW, Rgba borderC) {
    component::Avatar* av =
        component::Avatar::New(a)->Initials(Str(initials))->Size(px);
    if (radius >= 0) {
        av->Radius(radius);
    }
    if (borderW > 0) {
        av->Border(borderW, borderC);
    }
    return av->IntoEl();
}

static El* AvatarRow(Arena* a, StoryApp* app, const char** names, int n,
                     int limit, bool ellipsis) {
    float sz = component::AvatarSizePx(app->size);
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
        box->Child(component::Avatar::New(a)
                       ->Initials(StrL("\xE2\x8B\xAF"))
                       ->Bg(ThemeNow().secondary)
                       ->WithSize(app->size)
                       ->IntoEl()
                       ->Absolute()
                       ->Left(extraLeft));
    }
    for (int i = shown - 1; i >= 0; i--) {
        box->Child(component::Avatar::New(a)
                       ->Initials(Str(names[i]))
                       ->WithSize(app->size)
                       ->IntoEl()
                       ->Absolute()
                       ->Left(i * step));
    }
    return box;
}

El* AvatarRender(StoryApp* app, Arena* a) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(a, app));

    El* img = StorySection(a, "Image", "Use an image when one is available.");
    El* imgRow = Div(a)->FlexRow()->Gap(12)->ItemsCenter();
    imgRow->Child(Face(a, app, "JL"));
    imgRow->Child(Face(a, app, "HU"));
    StorySectionAdd(img, imgRow);
    page->Child(img);

    El* fb = StorySection(
        a, "Fallback", "Show initials or an icon when no image is available.");
    El* fbRow = Div(a)->FlexRow()->Gap(12)->ItemsCenter();
    fbRow->Child(Face(a, app, "JL"));
    fbRow->Child(component::Avatar::New(a)->WithSize(app->size)->IntoEl());
    fbRow->Child(component::Avatar::New(a)
                     ->Placeholder(IconName::Building2)
                     ->WithSize(app->size)
                     ->IntoEl());
    StorySectionAdd(fb, fbRow);
    page->Child(fb);

    static const char* kGroupA[] = {"JL", "HU", "TW", "AB", "CD", "EF"};
    static const char* kGroupB[] = {"JL", "HU", "TW", "AB", "CD", "EF",
                                    "GH", "IJ", "KL", "MN", "OP"};
    El* grp = StorySection(
        a, "Group", "Groups can limit visible avatars and show overflow.");
    El* grpCol = Div(a)->FlexCol()->Gap(20)->ItemsCenter();
    grpCol->Child(AvatarRow(a, app, kGroupA, 6, 3, false));
    grpCol->Child(AvatarRow(a, app, kGroupB, 11, 5, true));
    StorySectionAdd(grp, grpCol);
    page->Child(grp);

    El* shape = StorySection(a, "Custom shape",
                             "Set an explicit size and corner radius.");
    StorySectionAdd(shape, FacePx(a, "JL", 100, 20, 1, ThemeNow().border));
    page->Child(shape);

    El* style = StorySection(a, "Custom style",
                             "Add borders and shadows to the image.");
    StorySectionAdd(style, FacePx(a, "TW", 100, -1, 3, ThemeNow().foreground));
    page->Child(style);
    return page;
}

void AvatarClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryAvatar, AvatarRender, AvatarClick);
