#include "Story.h"

static El* Face(Ctx* cx, StoryApp* app, const char* initials) {
    Arena* a = cx->a;
    return component::Avatar::New(cx)
        ->Initials(Str(initials))
        ->WithSize(app->size)
        ->IntoEl();
}

static El* FaceLarge(Ctx* cx, StoryApp* app, const char* initials) {
    Arena* a = cx->a;
    return component::Avatar::New(cx)
        ->Initials(Str(initials))
        ->WithSize(UiSize::Large)
        ->IntoEl();
}

static El* FaceSmall(Ctx* cx, StoryApp* app, const char* initials) {
    Arena* a = cx->a;
    (void)app;
    return component::Avatar::New(cx)
        ->Initials(Str(initials))
        ->WithSize(UiSize::Small)
        ->IntoEl();
}

El* BadgeRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(cx, app));

    El* icons = StorySection(cx, "Icon", nullptr);
    El* iconRow = Div(a)->FlexRow()->Gap(24)->ItemsCenter();
    iconRow->Child(component::Badge::New(cx)
                       ->Count(3)
                       ->WithSize(app->size)
                       ->Child(IconEl(a, IconName::Bell, UiSizePx(app->size)))
                       ->IntoEl());
    iconRow->Child(component::Badge::New(cx)
                       ->Count(103)
                       ->WithSize(app->size)
                       ->Child(IconEl(a, IconName::Inbox, UiSizePx(app->size)))
                       ->IntoEl());
    StorySectionAdd(icons, iconRow);
    page->Child(icons);

    El* counts = StorySection(cx, "Count", nullptr);
    El* countRow = Div(a)->FlexRow()->Gap(24)->ItemsCenter();
    countRow->Child(component::Badge::New(cx)
                        ->Count(3)
                        ->WithSize(app->size)
                        ->Child(Face(cx, app, "JL"))
                        ->IntoEl());
    countRow->Child(component::Badge::New(cx)
                        ->Count(103)
                        ->WithSize(app->size)
                        ->Child(Face(cx, app, "HU"))
                        ->IntoEl());
    StorySectionAdd(counts, countRow);
    page->Child(counts);

    El* ic = StorySection(cx, "Badge icon", nullptr);
    El* icRow = Div(a)->FlexRow()->Gap(24)->ItemsCenter();
    icRow->Child(component::Badge::New(cx)
                     ->Icon(IconName::Check)
                     ->Color(Rgb(0x22, 0xd3, 0xee))
                     ->WithSize(app->size)
                     ->Child(Face(cx, app, "JL"))
                     ->IntoEl());
    icRow->Child(component::Badge::New(cx)
                     ->Icon(IconName::Star)
                     ->Color(th.yellow)
                     ->WithSize(app->size)
                     ->Child(Face(cx, app, "TW"))
                     ->IntoEl());
    StorySectionAdd(ic, icRow);
    page->Child(ic);

    El* dots = StorySection(cx, "Dot", nullptr);
    StorySectionAdd(dots, component::Badge::New(cx)
                              ->Dot()
                              ->Count(1)
                              ->WithSize(app->size)
                              ->Child(Face(cx, app, "JL"))
                              ->IntoEl());
    page->Child(dots);

    El* color = StorySection(cx, "Color", nullptr);
    El* colorRow = Div(a)->FlexRow()->Gap(24)->ItemsCenter();
    colorRow->Child(component::Badge::New(cx)
                        ->Count(3)
                        ->Color(th.blue)
                        ->WithSize(app->size)
                        ->Child(Face(cx, app, "JL"))
                        ->IntoEl());
    colorRow->Child(component::Badge::New(cx)
                        ->Dot()
                        ->Color(th.green)
                        ->Count(1)
                        ->WithSize(app->size)
                        ->Child(Face(cx, app, "JL"))
                        ->IntoEl());
    StorySectionAdd(color, colorRow);
    page->Child(color);

    El* nest = StorySection(cx, "Nested", nullptr);
    El* nestRow = Div(a)->FlexRow()->Gap(24)->ItemsCenter();
    nestRow->Child(component::Badge::New(cx)
                       ->Count(212)
                       ->WithSize(UiSize::Large)
                       ->Child(component::Badge::New(cx)
                                   ->Icon(IconName::Check)
                                   ->Color(Rgb(0x22, 0xd3, 0xee))
                                   ->WithSize(app->size)
                                   ->Child(Face(cx, app, "JL"))
                                   ->IntoEl())
                       ->IntoEl());
    nestRow->Child(component::Badge::New(cx)
                       ->Count(2)
                       ->Color(th.green)
                       ->WithSize(UiSize::Large)
                       ->Child(component::Badge::New(cx)
                                   ->Icon(IconName::Star)
                                   ->Color(th.yellow)
                                   ->WithSize(app->size)
                                   ->Child(FaceLarge(cx, app, "TW"))
                                   ->IntoEl())
                       ->IntoEl());
    nestRow->Child(component::Badge::New(cx)
                       ->Count(3)
                       ->Color(th.green)
                       ->WithSize(app->size)
                       ->Child(component::Badge::New(cx)
                                   ->Icon(IconName::Asterisk)
                                   ->Color(th.green)
                                   ->WithSize(app->size)
                                   ->Child(Face(cx, app, "AB"))
                                   ->IntoEl())
                       ->IntoEl());
    nestRow->Child(component::Badge::New(cx)
                       ->Dot()
                       ->WithSize(app->size)
                       ->Child(component::Badge::New(cx)
                                   ->Icon(IconName::Sun)
                                   ->Color(th.red)
                                   ->WithSize(app->size)
                                   ->Child(FaceSmall(cx, app, "CD"))
                                   ->IntoEl())
                       ->IntoEl());
    StorySectionAdd(nest, nestRow);
    page->Child(nest);
    return page;
}

void BadgeClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryBadge, BadgeRender, BadgeClick);
