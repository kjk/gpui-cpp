#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

enum {
    ClickEditor = 360
};

static const char* kEditorDefault =
    "use std::collections::HashMap;\n"
    "\n"
    "#[derive(Debug, Clone)]\n"
    "struct Workspace {\n"
    "    name: String,\n"
    "    files: HashMap<String, usize>,\n"
    "}\n"
    "\n"
    "impl Workspace {\n"
    "    fn new(name: impl Into<String>) -> Self {\n"
    "        Self {\n"
    "            name: name.into(),\n"
    "            files: HashMap::new(),\n"
    "        }\n"
    "    }\n"
    "\n"
    "    fn index(&mut self, path: &str, lines: usize) {\n"
    "        // Keep the latest line count for each source file.\n"
    "        self.files.insert(path.to_owned(), lines);\n"
    "    }\n"
    "\n"
    "    fn summary(&self) -> String {\n"
    "        let total: usize = self.files.values().sum();\n"
    "        format!(\"{}: {} files, {total} lines\", self.name, "
    "self.files.len())\n"
    "    }\n"
    "}\n"
    "\n"
    "fn main() {\n"
    "    let mut workspace = Workspace::new(\"gpui-component\");\n"
    "    workspace.index(\"src/main.rs\", 128);\n"
    "    workspace.index(\"src/editor.rs\", 372);\n"
    "    println!(\"{}\", workspace.summary());\n"
    "}\n";

static void OnEditor(ShowcaseApp* app, Ctx* cx, const ClickEvent*) {
    (void)app;
    app->editorOn = true;
    app->textareaOn = false;
    app->input.focused = false;
    Notify(cx);
}

El* ShowcaseEditor(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    if (!app->editorInited) {
        StrCopyZ(app->editor, (int)sizeof(app->editor), kEditorDefault);
        app->editorLen = (int)strlen(app->editor);
        app->editorCursor = 0;
        app->editorInited = true;
    }
    bool blink = app->editorOn && ((int)(TimeNow() * 2.0) % 2 == 0);
    return Div(a)
        ->FlexCol()
        ->W(320)
        ->Gap(4)
        ->ItemsStart()
        ->Child(Div(a)->H(16)->ItemsCenter()->Child(
            TextEl(a, StrL("Rust Editor"))
                ->Font(12)
                ->Fg(Rgb(0x17, 0x17, 0x17))))
        ->Child(InputBase::New(cx, StrL("example-editor"), ClickEditor)
                    ->OnClick(Listen(cx, &OnEditor))
                    ->W(320)
                    ->H(128)
                    ->PadX(8)
                    ->PadY(8)
                    ->ClipY()
                    ->FlexCol()
                    ->FocusId(0)
                    ->Border(1, app->editorOn ? Rgb(0x17, 0x17, 0x17)
                                              : Rgb(0xd4, 0xd4, 0xd4))
                    ->Child(Editor::New(cx, app->editor, app->editorCursor,
                                        blink)));
}

SHOWCASE_PAGE(CompEditor, ShowcaseEditor);
