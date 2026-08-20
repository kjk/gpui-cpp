#include "Story.h"

// editor_preview.rs, the sample the Rust story loads into its editor.
static const char* kEditorCode =
    "use gpui::{Context, IntoElement, ParentElement, Render, Styled, Window, "
    "div};\n"
    "use gpui_component::{ActiveTheme, Icon, IconName, StyledExt, h_flex, "
    "progress::Progress, v_flex};\n"
    "\n"
    "pub struct ProjectOverview {\n"
    "    progress: f32,\n"
    "}\n"
    "\n"
    "impl ProjectOverview {\n"
    "    pub fn new() -> Self {\n"
    "        Self { progress: 72. }\n"
    "    }\n"
    "\n"
    "    fn metric(&self, label: &'static str, value: &'static str) -> impl "
    "IntoElement {\n"
    "        v_flex()\n"
    "            .gap_1()\n"
    "            .p_4()\n"
    "            .rounded_lg()\n"
    "            .border_1()\n"
    "            .child(div().text_sm().child(label))\n"
    "            .child(div().text_2xl().font_semibold().child(value))\n"
    "    }\n"
    "}\n"
    "\n"
    "impl Render for ProjectOverview {\n"
    "    fn render(&mut self, _: &mut Window, cx: &mut Context<Self>) -> impl "
    "IntoElement {\n"
    "        v_flex()\n"
    "            .size_full()\n"
    "            .gap_5()\n"
    "            .p_6()\n"
    "            .child(\n"
    "                h_flex()\n"
    "                    .items_center()\n"
    "                    .justify_between()\n"
    "                    .child(\n"
    "                        v_flex()\n"
    "                            .gap_1()\n"
    "                            "
    ".child(div().text_2xl().font_semibold().child(\"Project overview\"))\n"
    "                            .child(div().text_sm().child(\"Everything is "
    "moving on schedule.\")),\n"
    "                    )\n"
    "                    .child(Icon::new(IconName::ChartNoAxesCombined)),\n"
    "            )\n"
    "            .child(\n"
    "                h_flex()\n"
    "                    .gap_3()\n"
    "                    .child(self.metric(\"Open tasks\", \"24\"))\n"
    "                    .child(self.metric(\"Completed\", \"86%\"))\n"
    "                    .child(self.metric(\"Contributors\", \"12\")),\n"
    "            )\n"
    "            .child(\n"
    "                v_flex()\n"
    "                    .gap_3()\n"
    "                    .p_4()\n"
    "                    .rounded_lg()\n"
    "                    .bg(cx.theme().muted)\n"
    "                    .child(\n"
    "                        h_flex()\n"
    "                            .justify_between()\n"
    "                            .child(\"Release progress\")\n"
    "                            .child(format!(\"{}%\", self.progress)),\n"
    "                    )\n"
    "                    "
    ".child(Progress::new(\"release\").value(self.progress)),\n"
    "            )\n"
    "    }\n"
    "}\n"
    "\n";

// The decorations tab shows a short document instead.
static const char* kDecorationText =
    "Decoration styles\n"
    "Color\n"
    "Italic\n";

struct EditorStory {
    int tab = 0;
    bool readOnly = false;
    // One EditorState per tab, the way the Rust story keeps one per document.
    InputState code;
    InputState decorations;
    bool seeded = false;

    static El* Render(EditorStory* self, Ctx* cx);
};

static void SetEditorTab(EditorStory* self, Ctx* cx, const ClickEvent*,
                         intptr_t ix) {
    self->tab = (int)ix;
    Notify(cx);
}
static void ToggleReadOnly(EditorStory* self, Ctx* cx, const ClickEvent*) {
    self->readOnly = !self->readOnly;
    Notify(cx);
}

El* EditorStory::Render(EditorStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    if (!self->seeded) {
        self->seeded = true;
        self->code.kind = InputKind::Editor;
        self->decorations.kind = InputKind::Editor;
        InputSetValue(&self->code, Str(kEditorCode));
        InputSetValue(&self->decorations, Str(kDecorationText));
    }
    self->code.readonly = self->readOnly;
    self->decorations.readonly = self->readOnly;
    El* page = Div(a)->FlexCol()->Gap(12)->W(kFill);

    // The tab bar on the left, the read-only switch on the right.
    El* head = Div(a)->FlexRow()->W(kFill)->ItemsCenter()->JustifyBetween();
    head->Child(Div(a)->W(256)->Child(component::Tabs::New(cx)
                                          ->Tab(StrL("Code"))
                                          ->Tab(StrL("Decorations"))
                                          ->Selected(self->tab)
                                          ->OnChange(Listen(cx, &SetEditorTab))
                                          ->IntoEl()));
    head->Child(component::Switch::New(cx, StrL("editor-read-only"))
                    ->Label(StrL("Read only"))
                    ->Checked(self->readOnly)
                    ->OnClick(Listen(cx, &ToggleReadOnly))
                    ->IntoEl());
    page->Child(head);

    El* box =
        Div(a)->FlexCol()->W(kFill)->Radius(th.radius)->Border(1, th.border);
    // The editor owns the box its rows scroll inside, so the caret can bring
    // the view with it.
    box->Child(component::Highlighter::New(
                   cx, StrL("editor"),
                   self->tab == 0 ? &self->code : &self->decorations)
                   ->H(WindowSize(cx->win).dipH - 262)
                   ->IntoEl());
    page->Child(box);
    return page;
}

STORY_PAGE(StoryEditor, EditorStory);
