#include "Story.h"

struct EditorStory {
    static El* Render(EditorStory* self, Ctx* cx);
};

El* EditorStory::Render(EditorStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(cx, "Code", "A code editor with line numbers.");
    StorySectionAdd(sec, component::Highlighter::New(
                             cx, "fn main() {\n    println!(\"hello\");\n}\n")
                             ->IntoEl());
    page->Child(sec);
    return page;
}

STORY_PAGE(StoryEditor, EditorStory);
