#include "Story.h"

struct SpinnerStory {
    StoryToolbarState toolbar;

    static El* Render(SpinnerStory* self, Ctx* cx);
};

// section(title).description(..).gap_x_2(): every section here narrows the
// row gap and takes its spinners as direct children, so they wrap the way the
// section's own h_flex wraps them.
static El* SpinnerSection(Ctx* cx, const char* title, const char* desc) {
    El* sec = StorySection(cx, title, desc);
    StorySectionBody(sec)->GapX(8);
    return sec;
}

El* SpinnerStory::Render(SpinnerStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    UiSize size = self->toolbar.size;
    El* page = Div(a)->FlexCol()->Gap(12)->W(kFill);
    page->Child(StoryToolbar(cx, self));

    El* def =
        SpinnerSection(cx, "Default", "An indeterminate loading indicator.");
    StorySectionAdd(def, component::Spinner::New(cx)->WithSize(size)->IntoEl());
    page->Child(def);

    El* color = SpinnerSection(
        cx, "Color", "Use a color that suits the surrounding status.");
    StorySectionAdd(
        color,
        component::Spinner::New(cx)->WithSize(size)->Color(th.blue)->IntoEl());
    StorySectionAdd(
        color,
        component::Spinner::New(cx)->WithSize(size)->Color(th.green)->IntoEl());
    page->Child(color);

    El* csz = SpinnerSection(cx, "Custom size",
                             "A fixed pixel size is also supported.");
    StorySectionAdd(csz, component::Spinner::New(cx)->Size(64)->IntoEl());
    page->Child(csz);

    El* ic = SpinnerSection(cx, "Icon", "Replace the default spinner glyph.");
    StorySectionAdd(ic, component::Spinner::New(cx)
                            ->WithSize(size)
                            ->Icon(IconName::LoaderCircle)
                            ->IntoEl());
    StorySectionAdd(ic, component::Spinner::New(cx)
                            ->WithSize(size)
                            ->Icon(IconName::LoaderCircle)
                            ->Color(th.cyan)
                            ->IntoEl());
    page->Child(ic);

    El* ease =
        SpinnerSection(cx, "Easing", "Customize the rotation timing curve.");
    // The three curves the Rust story shows: linear, bounce(ease_in_out) —
    // which turns one way and then back — and ease_out_quint. Each spinner
    // names itself, so they keep their own phase rather than sharing one.
    struct EaseSpec {
        const char* id;
        EaseFn fn;
    };
    static const EaseSpec kEases[3] = {{"spin-linear", EaseLinear},
                                       {"spin-bounce", EaseBounceInOut},
                                       {"spin-quint", EaseOutQuint}};
    for (int i = 0; i < 3; i++) {
        StorySectionAdd(ease, component::Spinner::New(cx)
                                  ->WithSize(size)
                                  ->Icon(IconName::Loader)
                                  ->Id(Str(kEases[i].id))
                                  ->Ease(kEases[i].fn)
                                  ->IntoEl());
    }
    page->Child(ease);
    return page;
}

STORY_PAGE(StorySpinner, SpinnerStory);
