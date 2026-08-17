#include "Story.h"

El* DataTableRender(StoryApp* app, Arena* a) {
    (void)app;
    static const char* heads[] = {"Name", "Email", "Role"};
    static const char* r0[] = {"Ada Lovelace", "ada@example.com", "Admin"};
    static const char* r1[] = {"Alan Turing", "alan@example.com", "Editor"};
    static const char* r2[] = {"Grace Hopper", "grace@example.com", "Viewer"};
    static const char* r3[] = {"Donald Knuth", "don@example.com", "Editor"};
    static const char** rows[] = {r0, r1, r2, r3};
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default", "Powerful table and datagrids.");
    StorySectionAdd(sec, component::Table::New(a)->Heads(heads, 3)->Rows(rows, 4)->IntoEl());
    page->Child(sec);
    return page;
}

void DataTableClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryDataTable, DataTableRender, DataTableClick);
