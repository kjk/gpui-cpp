#include "Story.h"

struct DataTableStory {
    static El* Render(DataTableStory* self, Ctx* cx);
    static void Click(DataTableStory* self, Ctx* cx, int id);
};

El* DataTableStory::Render(DataTableStory* self, Ctx* cx) {
    Arena* a = cx->a;
    static const char* heads[] = {"Name", "Email", "Role"};
    static const char* r0[] = {"Ada Lovelace", "ada@example.com", "Admin"};
    static const char* r1[] = {"Alan Turing", "alan@example.com", "Editor"};
    static const char* r2[] = {"Grace Hopper", "grace@example.com", "Viewer"};
    static const char* r3[] = {"Donald Knuth", "don@example.com", "Editor"};
    static const char** rows[] = {r0, r1, r2, r3};
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(cx, "Default", "Powerful table and datagrids.");
    StorySectionAdd(
        sec,
        component::Table::New(cx)->Heads(heads, 3)->Rows(rows, 4)->IntoEl());
    page->Child(sec);
    return page;
}

void DataTableStory::Click(DataTableStory* self, Ctx* cx, int id) {
    (void)cx;
    (void)id;
}

STORY_PAGE(StoryDataTable, DataTableStory);
