/* taffy's `#[derive(PartialEq)]` on Style, which src/gpui's layout cache asks
 * the question through: a style that compares equal is a node taffy is not
 * told about, and a node it is not told about keeps last frame's layout.
 *
 * A wrong answer here is a stale frame, and the one way to get one is a field
 * the comparison forgot — so this walks the fields a `gpui::Style` can reach
 * and asserts that moving each of them is visible. */

#include "Test.h"

// `Style` is a name in both namespaces and means different things in each,
// which is why src/taffy has one of its own; here it is taffy's.
using taffy::Dimension;
using taffy::LengthPercentage;
using taffy::LengthPercentageAuto;
using TStyle = taffy::Style;

static void TwoDefaultStylesAreEqual() {
    TStyle a;
    TStyle b;
    utassert(a == b);
    utassert(!(a != b));
}

// One field at a time, over everything the style translation writes.
static void EveryFieldIsCompared() {
    TStyle base;

    {
        TStyle s = base;
        s.display = taffy::Display::Block;
        utassert(!(s == base));
    }
    {
        TStyle s = base;
        s.boxSizing = taffy::BoxSizing::ContentBox;
        utassert(!(s == base));
    }
    {
        TStyle s = base;
        s.overflow.x = taffy::Overflow::Scroll;
        utassert(!(s == base));
    }
    {
        TStyle s = base;
        s.scrollbarWidth = 8.f;
        utassert(!(s == base));
    }
    {
        TStyle s = base;
        s.position = taffy::Position::Absolute;
        utassert(!(s == base));
    }
    {
        TStyle s = base;
        s.inset.left = LengthPercentageAuto::Length(4.f);
        utassert(!(s == base));
    }
    {
        TStyle s = base;
        s.size.width = Dimension::Length(10.f);
        utassert(!(s == base));
    }
    {
        TStyle s = base;
        s.minSize.height = Dimension::Length(1.f);
        utassert(!(s == base));
    }
    {
        TStyle s = base;
        s.maxSize.width = Dimension::Percent(0.5f);
        utassert(!(s == base));
    }
    {
        TStyle s = base;
        s.aspectRatio = 1.5f;
        utassert(!(s == base));
    }
    {
        TStyle s = base;
        s.margin.top = LengthPercentageAuto::Length(2.f);
        utassert(!(s == base));
    }
    {
        TStyle s = base;
        s.padding.bottom = LengthPercentage::Length(3.f);
        utassert(!(s == base));
    }
    {
        TStyle s = base;
        s.border.right = LengthPercentage::Length(1.f);
        utassert(!(s == base));
    }
    {
        TStyle s = base;
        s.alignItems = taffy::OptAlignItems(
            taffy::AlignItems{taffy::AlignItemsKeyword::Center});
        utassert(!(s == base));
    }
    {
        TStyle s = base;
        s.alignSelf = taffy::OptAlignSelf(
            taffy::AlignSelf{taffy::AlignItemsKeyword::Start,
                             taffy::AlignmentSafety::Safe});
        utassert(!(s == base));
    }
    {
        TStyle s = base;
        s.justifyContent = taffy::OptJustifyContent(taffy::JustifyContent{
            taffy::AlignContentKeyword::SpaceBetween});
        utassert(!(s == base));
    }
    {
        TStyle s = base;
        s.gap.width = LengthPercentage::Length(8.f);
        utassert(!(s == base));
    }
    {
        TStyle s = base;
        s.textAlign = taffy::TextAlign::LegacyCenter;
        utassert(!(s == base));
    }
    {
        TStyle s = base;
        s.flexDirection = taffy::FlexDirection::Column;
        utassert(!(s == base));
    }
    {
        TStyle s = base;
        s.flexWrap = taffy::FlexWrap::Wrap;
        utassert(!(s == base));
    }
    {
        TStyle s = base;
        s.flexBasis = Dimension::Length(0.f);
        utassert(!(s == base));
    }
    {
        TStyle s = base;
        s.flexGrow = 1.f;
        utassert(!(s == base));
    }
    {
        TStyle s = base;
        s.flexShrink = 0.f;
        utassert(!(s == base));
    }
    {
        TStyle s = base;
        s.gridAutoFlow = taffy::GridAutoFlow::Column;
        utassert(!(s == base));
    }
    {
        TStyle s = base;
        s.gridRow.start = taffy::GridPlacement::FromLineIndex(2);
        utassert(!(s == base));
    }
    {
        TStyle s = base;
        s.gridColumn.end = taffy::GridPlacement::FromSpan(3);
        utassert(!(s == base));
    }
}

// A Vec in Rust, so what is compared is the contents and not the address —
// the element tree hands layout a fresh copy of a grid template every frame.
static void ATemplateIsComparedByItsContents() {
    taffy::TrackSizingFunction rowsA[2] = {
        taffy::TrackSizingFunction::Length(10.f),
        taffy::TrackSizingFunction::Auto()};
    taffy::TrackSizingFunction rowsB[2] = {
        taffy::TrackSizingFunction::Length(10.f),
        taffy::TrackSizingFunction::Auto()};
    taffy::TrackSizingFunction rowsC[2] = {
        taffy::TrackSizingFunction::Length(11.f),
        taffy::TrackSizingFunction::Auto()};

    TStyle a;
    TStyle b;
    a.gridAutoRows = taffy::Slice<taffy::TrackSizingFunction>{rowsA, 2};
    b.gridAutoRows = taffy::Slice<taffy::TrackSizingFunction>{rowsB, 2};
    utassert(a == b);

    b.gridAutoRows = taffy::Slice<taffy::TrackSizingFunction>{rowsC, 2};
    utassert(!(a == b));

    b.gridAutoRows = taffy::Slice<taffy::TrackSizingFunction>{rowsA, 1};
    utassert(!(a == b));
}

// `Optf` is Rust's `Option<f32>` with a NaN standing for None. Compared as
// floats, two Nones would come out unequal and every node with no aspect
// ratio would lay out again every frame.
static void TwoNonesAreEqual() {
    TStyle a;
    TStyle b;
    utassert(taffy::IsNone(a.aspectRatio) && taffy::IsNone(b.aspectRatio));
    utassert(a == b);

    a.aspectRatio = 2.f;
    b.aspectRatio = 2.f;
    utassert(a == b);
}

void TestStyleEq() {
    TestSuite("style_eq");
    TwoDefaultStylesAreEqual();
    EveryFieldIsCompared();
    ATemplateIsComparedByItsContents();
    TwoNonesAreEqual();
}
