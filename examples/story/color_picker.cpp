#include "Story.h"

struct ColorPickerStory {
    // gpui_base::ColorPickerState: the committed color and the transient one
    // a hover shows beside it. Everything on this page reads the preview when
    // there is one and the value otherwise.
    // Seeded with indigo_500, the way the Rust story seeds its state.
    ColorPickerState color = {0x6366f1, true, 0, false, false, 0};
    bool colorOpen = false;
    StoryToolbarState toolbar;
    static El* Render(ColorPickerStory* self, Ctx* cx);
    static void OnKey(ColorPickerStory* self, Ctx* cx, const KeyEvent* ev);
};

static void ToggleColor(ColorPickerStory* self, Ctx* cx, const ClickEvent*) {
    self->colorOpen = !self->colorOpen;
    Notify(cx);
}
static void SetColor(ColorPickerStory* self, Ctx* cx, const ClickEvent*,
                     intptr_t hex) {
    ColorPickerSetValue(&self->color, (uint32_t)hex);
    self->colorOpen = false;
    Notify(cx);
}
// preview_color while the pointer is on a swatch, clear_preview when it
// leaves. The clear reports whether anything changed, which is Rust's early
// return when the preview is already what is committed.
static void PreviewColor(ColorPickerStory* self, Ctx* cx, const HoverEvent* ev,
                         intptr_t hex) {
    if (ev->hovered) {
        ColorPickerPreview(&self->color, (uint32_t)hex);
    } else if (!ColorPickerClearPreview(&self->color)) {
        return;
    }
    Notify(cx);
}

// What the picker shows: the preview if one is up, the value otherwise.
static uint32_t ShownHex(const ColorPickerStory* self) {
    uint32_t hex = 0;
    ColorPickerShown(&self->color, &hex);
    return hex;
}

El* ColorPickerStory::Render(ColorPickerStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    uint32_t shown = ShownHex(self);
    Rgba color = Rgb((uint8_t)((shown >> 16) & 0xff),
                     (uint8_t)((shown >> 8) & 0xff), (uint8_t)(shown & 0xff));

    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill)->ItemsCenter();
    page->Child(StoryToolbar(cx, self));

    El* sec = StorySection(cx, "Theme Color",
                           "Select a color and preview the resulting value.");
    StorySectionBody(sec)->W(440);
    El* card = Div(a)
                   ->FlexCol()
                   ->W(kFill)
                   ->Gap(16)
                   ->Pad(16)
                   ->Radius(th.radiusLg)
                   ->Border(1, th.border);

    El* head =
        Div(a)->FlexRow()->W(kFill)->Gap(16)->ItemsCenter()->JustifyBetween();
    El* text = Div(a)->FlexCol()->Gap(4);
    text->Child(StoryTxt(cx, StrL("Accent color"), 16, th.foreground)
                    ->Medium());
    text->Child(StoryTxt(cx, StrL("Used for primary actions and highlights."),
                         14, th.mutedFg));
    head->Child(text);
    head->Child(component::ColorPicker::New(cx)
                    ->Hex(shown)
                    ->Open(self->colorOpen)
                    ->OnToggle(Listen(cx, &ToggleColor))
                    ->OnChange(Listen(cx, &SetColor))
                    ->OnPreview(Listen(cx, &PreviewColor))
                    ->IntoEl());
    card->Child(head);

    // The preview: the color over a muted footer naming its hex.
    El* preview = Div(a)
                      ->FlexCol()
                      ->W(kFill)
                      ->ClipY()
                      ->Radius(th.radiusLg)
                      ->Border(1, th.border);
    preview->Child(Div(a)->W(kFill)->H(96)->Bg(color));
    El* foot = Div(a)
                   ->FlexRow()
                   ->W(kFill)
                   ->PadX(12)
                   ->PadY(8)
                   ->ItemsCenter()
                   ->JustifyBetween()
                   ->Bg(th.muted);
    foot->Child(StoryTxt(cx, StrL("Selected color"), 14, th.mutedFg));
    foot->Child(
        StoryTxt(cx, StoryFmt(cx, "#%06X", shown & 0xffffff), 16, th.foreground)
            ->Mono()
            ->Medium());
    preview->Child(foot);
    card->Child(preview);

    StorySectionAdd(sec, card);
    page->Child(sec);
    return page;
}

// Esc closes what this page has open, like an overlay dismiss.
void ColorPickerStory::OnKey(ColorPickerStory* self, Ctx* cx,
                             const KeyEvent* ev) {
    if (ev->vk != KeyEscape) {
        return;
    }
    self->colorOpen = false;
    Notify(cx);
}

STORY_PAGE_KEYS(StoryColorPicker, ColorPickerStory);
