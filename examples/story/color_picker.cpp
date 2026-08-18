#include "Story.h"

struct ColorPickerStory {
    // indigo_500, the way the Rust story seeds its state.
    uint32_t colorHex = 0x6366f0;
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
    self->colorHex = (uint32_t)hex;
    self->colorOpen = false;
    Notify(cx);
}

El* ColorPickerStory::Render(ColorPickerStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    Rgba color = Rgb((uint8_t)((self->colorHex >> 16) & 0xff),
                     (uint8_t)((self->colorHex >> 8) & 0xff),
                     (uint8_t)(self->colorHex & 0xff));

    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill)->ItemsCenter();
    page->Child(StoryToolbar(cx, self));

    El* sec = StorySection(cx, "Theme Color",
                           "Select a color and preview the resulting value.");
    El* card = Div(a)
                   ->FlexCol()
                   ->W(440)
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
                    ->Hex(self->colorHex)
                    ->Open(self->colorOpen)
                    ->OnToggle(Listen(cx, &ToggleColor))
                    ->OnChange(Listen(cx, &SetColor))
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
    foot->Child(StoryTxt(cx, StoryFmt(cx, "#%06X", self->colorHex & 0xffffff),
                         16, th.foreground)
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
    if (ev->vk != VK_ESCAPE) {
        return;
    }
    self->colorOpen = false;
    Notify(cx);
}

STORY_PAGE_KEYS(StoryColorPicker, ColorPickerStory);
