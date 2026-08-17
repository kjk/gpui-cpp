#include "Story.h"

El* BadgeRender(StoryApp* app, Arena* a) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(a, app));

    El* icons = StorySection(a, "Icon", "Count badges over an icon.");
    El* row = Div(a)->FlexRow()->Gap(24)->ItemsCenter();
    row->Child(component::Badge::New(a)
                   ->Count(3)
                   ->WithSize(app->size)
                   ->Child(IconEl(a, IconName::Inbox, UiSizePx(app->size)))
                   ->IntoEl());
    row->Child(component::Badge::New(a)
                   ->Count(103)
                   ->WithSize(app->size)
                   ->Child(IconEl(a, IconName::Inbox, UiSizePx(app->size)))
                   ->IntoEl());
    StorySectionAdd(icons, row);
    page->Child(icons);

    El* dots = StorySection(a, "Dot", "A status dot without a count.");
    StorySectionAdd(dots, component::Badge::New(a)
                              ->Dot()
                              ->Child(IconEl(a, IconName::Inbox, 24))
                              ->IntoEl());
    page->Child(dots);
    return page;
}

void BadgeClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryBadge, BadgeRender, BadgeClick);
