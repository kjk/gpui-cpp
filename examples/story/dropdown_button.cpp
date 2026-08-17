#include "Story.h"

enum {
    ClickStoryDrop = 2750
};

El* DropdownButtonRender(StoryApp* app, Arena* a) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default",
                           "A button that opens a dropdown menu of actions.");
    El* col = Div(a)->FlexCol()->Gap(4);
    col->Child(component::Button::New(a, StrL("drop-btn"))
                   ->Label(StrL("Actions"))
                   ->Secondary()
                   ->IntoEl());
    if (app->selectOpen) {
        col->Child(component::Menu::New(a)
                       ->Item(StrL("Duplicate"))
                       ->Item(StrL("Move"))
                       ->Item(StrL("Delete"))
                       ->IntoEl());
    }
    StorySectionAdd(sec, col);
    page->Child(sec);
    return page;
}

void DropdownButtonClick(StoryApp* app, int id) {
    if (id == ClickStoryDrop || id == HashClickId(StrL("drop-btn"))) {
        app->selectOpen = !app->selectOpen;
    }
}

STORY_PAGE(StoryDropdownButton, DropdownButtonRender, DropdownButtonClick);
