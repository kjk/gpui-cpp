#include "Story.h"

El* EditorRender(StoryApp* app, Arena* a) {
    (void)app;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Rust", "A code editor with line numbers.");
    StorySectionAdd(sec, component::Highlighter::New(
                             a, "fn main() {\n    println!(\"hello\");\n}\n")
                             ->IntoEl());
    page->Child(sec);
    return page;
}

void EditorClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryEditor, EditorRender, EditorClick);
