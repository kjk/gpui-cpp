#include "Story.h"

enum {
    ClickDropDefault = 2750,
    ClickDropOutline,
    ClickDropGhost
};

static El* DropMenu(Ctx* cx) {
    Arena* a = cx->a;
    return component::Menu::New(cx)
        ->Item(StrL("Disabled"))
        ->Item(StrL("Loading"))
        ->Item(StrL("Selected"))
        ->Item(StrL("Compact"))
        ->IntoEl();
}

static El* DropBlock(Ctx* cx, StoryApp* app, int which,
                     component::Button* btn) {
    Arena* a = cx->a;
    El* col = Div(a)->FlexCol()->Gap(4);
    col->Child(btn->DropdownCaret()->IntoEl()->Click(2750 + which));
    if (app->selectIx == which && app->selectOpen) {
        col->Child(DropMenu(cx));
    }
    return col;
}

El* DropdownButtonRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(cx, app));

    El* def =
        StorySection(cx, "Default", "A primary action with an attached menu.");
    StorySectionAdd(def, DropBlock(cx, app, 0,
                                   component::Button::New(cx, StrL("btn0"))
                                       ->Label(StrL("Primary Dropdown"))
                                       ->Primary()
                                       ->WithSize(app->size)));
    page->Child(def);

    El* out = StorySection(cx, "Outline", nullptr);
    StorySectionAdd(out,
                    DropBlock(cx, app, 1,
                              component::Button::New(cx, StrL("btn-outline"))
                                  ->Label(StrL("Outline Dropdown"))
                                  ->Danger()
                                  ->Outline()
                                  ->WithSize(app->size)));
    page->Child(out);

    El* ghost = StorySection(cx, "Ghost", nullptr);
    StorySectionAdd(ghost,
                    DropBlock(cx, app, 2,
                              component::Button::New(cx, StrL("btn-ghost"))
                                  ->Label(StrL("Ghost Dropdown"))
                                  ->Ghost()
                                  ->WithSize(app->size)));
    page->Child(ghost);
    return page;
}

void DropdownButtonClick(StoryApp* app, int id) {
    int which = -1;
    if (id == ClickDropDefault || id == HashClickId(StrL("btn0"))) {
        which = 0;
    } else if (id == ClickDropOutline ||
               id == HashClickId(StrL("btn-outline"))) {
        which = 1;
    } else if (id == ClickDropGhost || id == HashClickId(StrL("btn-ghost"))) {
        which = 2;
    }
    if (which < 0) {
        return;
    }
    if (app->selectOpen && app->selectIx == which) {
        app->selectOpen = false;
    } else {
        app->selectOpen = true;
        app->selectIx = which;
    }
}

STORY_PAGE(StoryDropdownButton, DropdownButtonRender, DropdownButtonClick);
