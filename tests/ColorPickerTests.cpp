/* Ported from crates/base/src/color_picker.rs.
 *
 * The committed color and the transient preview beside it are the module's
 * interaction model: a hover previews, leaving restores, and only a click
 * commits. Rust's own cases there drive a window; these are the rules the
 * state follows underneath. */

#include "Test.h"

static void APreviewHidesTheValueWithoutReplacingIt() {
    ColorPickerState s;
    ColorPickerSetValue(&s, 0x6366f0);
    uint32_t shown = 0;
    utassert(ColorPickerShown(&s, &shown) && shown == 0x6366f0);

    ColorPickerPreview(&s, 0x16a34a);
    utassert(ColorPickerShown(&s, &shown) && shown == 0x16a34a);
    // The committed color is untouched underneath.
    utassert(s.value == 0x6366f0);

    utassert(ColorPickerClearPreview(&s));
    utassert(ColorPickerShown(&s, &shown) && shown == 0x6366f0);
}

static void ClearingAPreviewOfTheCommittedColorChangesNothing() {
    ColorPickerState s;
    ColorPickerSetValue(&s, 0x6366f0);
    ColorPickerPreview(&s, 0x6366f0);
    // Rust returns early here: the pointer crossed the swatch that is already
    // picked, so there is nothing to restore and nothing to repaint.
    utassert(!ColorPickerClearPreview(&s));
    utassert(!s.hasPreview);
    // And with no preview up at all there is still nothing to do.
    utassert(!ColorPickerClearPreview(&s));
}

static void CommittingDropsThePreview() {
    ColorPickerState s;
    ColorPickerSetValue(&s, 0x6366f0);
    ColorPickerPreview(&s, 0x16a34a);
    ColorPickerSetValue(&s, 0x16a34a);
    utassert(!s.hasPreview);
    uint32_t shown = 0;
    utassert(ColorPickerShown(&s, &shown) && shown == 0x16a34a);
}

static void NoValueAndNoPreviewShowsNothing() {
    ColorPickerState s;
    uint32_t shown = 0xdeadbeef;
    // Rust's None, which draws as the empty square.
    utassert(!ColorPickerShown(&s, &shown));

    ColorPickerSetValue(&s, 0x123456);
    ColorPickerClearValue(&s);
    utassert(!ColorPickerShown(&s, &shown));
    // A preview still shows over no value at all.
    ColorPickerPreview(&s, 0x16a34a);
    utassert(ColorPickerShown(&s, &shown) && shown == 0x16a34a);
}

void TestColorPicker() {
    TestSuite("color_picker");
    APreviewHidesTheValueWithoutReplacingIt();
    ClearingAPreviewOfTheCommittedColorChangesNothing();
    CommittingDropsThePreview();
    NoValueAndNoPreviewShowsNothing();
}
