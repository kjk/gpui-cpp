/* The flexbox layout algorithm — taffy/src/compute/flexbox.rs, which follows
 * https://www.w3.org/TR/css-flexbox-1/
 *
 * The step numbering in the comments is the spec's, kept from the Rust so the
 * two can be read side by side.
 */

#include "taffy/compute.h"

namespace taffy {
namespace {

// Rust's `Rect<bool>`, which flexbox only uses to remember which margins were
// `auto` before they were resolved to zero.
struct RectBool {
    bool left = false;
    bool right = false;
    bool top = false;
    bool bottom = false;

    bool MainStart(FlexDirection d) const { return IsRow(d) ? left : top; }
    bool MainEnd(FlexDirection d) const { return IsRow(d) ? right : bottom; }
    bool CrossStart(FlexDirection d) const { return IsRow(d) ? top : left; }
    bool CrossEnd(FlexDirection d) const { return IsRow(d) ? bottom : right; }
};

// The intermediate results of a flexbox calculation for a single item.
struct FlexItem {
    NodeId node;
    // The order of the node relative to its siblings.
    uint32_t order = 0;

    SizeOptF size;
    SizeOptF minSize;
    SizeOptF maxSize;
    AlignSelf alignSelf;

    PointOverflow overflow;
    float scrollbarWidth = 0.0f;
    float flexShrink = 0.0f;
    float flexGrow = 0.0f;

    // Differs from minSize above: this also accounts for content-based
    // automatic minimum sizes.
    float resolvedMinimumMainSize = 0.0f;

    RectOptF inset;
    RectF margin;
    RectBool marginIsAuto;
    RectF padding;
    RectF border;

    // The default size of this item, and the same minus padding and border.
    float flexBasis = 0.0f;
    float innerFlexBasis = 0.0f;
    // How far this item has deviated from its target size.
    float violation = 0.0f;
    bool frozen = false;

    // Either the max- or min-content flex fraction.
    // https://www.w3.org/TR/css-flexbox-1/#intrinsic-main-sizes
    float contentFlexFraction = 0.0f;

    SizeF hypotheticalInnerSize;
    SizeF hypotheticalOuterSize;
    SizeF targetSize;
    SizeF outerTargetSize;

    // The position of the bottom edge of this item.
    float baseline = 0.0f;

    // The relative position from the item's natural flow position, from
    // relative position values, alignment and justification. Excludes
    // margin/padding/border.
    float offsetMain = 0.0f;
    float offsetCross = 0.0f;

    // https://www.w3.org/TR/css-overflow-3/#scroll-container
    bool IsScroll() const {
        return IsScrollContainer(overflow.x) || IsScrollContainer(overflow.y);
    }
};

// A line of FlexItems. `items` points into the container's item vector, which
// is not resized once the lines have been collected.
struct FlexLine {
    FlexItem* items = nullptr;
    int count = 0;
    float crossSize = 0.0f;
    float offsetCross = 0.0f;
};

// Values cached for the duration of the algorithm.
struct AlgoConstants {
    FlexDirection dir = FlexDirection::Row;
    Direction layoutDirection = Direction::Ltr;
    bool isRow = true;
    bool isColumn = false;
    bool isWrap = false;
    bool isWrapReverse = false;

    SizeOptF minSize;
    SizeOptF maxSize;
    RectF margin;
    RectF border;
    // padding + border + scrollbar gutter.
    RectF contentBoxInset;
    PointF scrollbarGutter;
    SizeF gap;
    AlignItems alignItems;
    AlignContent alignContent;
    OptJustifyContent justifyContent;

    // The border-box and content-box sizes of the node being laid out, if
    // known.
    SizeOptF nodeOuterSize;
    SizeOptF nodeInnerSize;

    // The size of the virtual container holding the flex items, and of the
    // internal container.
    SizeF containerSize;
    SizeF innerContainerSize;
};

// The total space gaps take up in an axis, given the gap size and how many
// items (children or flex lines) they sit between.
float SumAxisGaps(float gap, int numItems) {
    // Gaps only exist between items: fewer than two items means no gaps,
    // otherwise there are (numItems - 1) of them.
    if (numItems <= 1) {
        return 0.0f;
    }
    return gap * (float)(numItems - 1);
}

// Rust's `Option::filter`.
Optf Filter(Optf v, bool keep) {
    return keep ? v : Optf();
}

// ─── compute_constants ───────────────────────────────────────────────────

AlgoConstants ComputeConstants(TaffyTree* tree, const Style& style,
                               SizeOptF knownDimensions, SizeOptF parentSize) {
    CalcResolver calc = tree->calc;
    AlgoConstants c;
    c.dir = style.flexDirection;
    c.isRow = IsRow(c.dir);
    c.isColumn = IsColumn(c.dir);
    c.isWrap = style.flexWrap == FlexWrap::Wrap ||
               style.flexWrap == FlexWrap::WrapReverse;
    c.isWrapReverse = style.flexWrap == FlexWrap::WrapReverse;

    Optf aspectRatio = style.aspectRatio;
    c.margin = style.margin.ResolveOrZero(parentSize.width, calc);
    RectF padding = style.padding.ResolveOrZero(parentSize.width, calc);
    c.border = style.border.ResolveOrZero(parentSize.width, calc);
    SizeF paddingBorderSum = padding.SumAxes() + c.border.SumAxes();
    SizeF boxSizingAdjustment = style.boxSizing == BoxSizing::ContentBox
                                    ? paddingBorderSum
                                    : SizeF::Zero();

    c.alignItems = style.alignItems
                       .UnwrapOr(AlignItems{AlignItemsKeyword::Stretch});
    c.alignContent = style.alignContent
                         .UnwrapOr(AlignContent{AlignContentKeyword::Stretch});
    c.justifyContent = style.justifyContent;
    c.layoutDirection = style.direction;

    // A node that scrolls vertically needs *horizontal* space reserved for a
    // scrollbar, hence the transposed axes.
    PointOverflow t = style.overflow.Transpose();
    c.scrollbarGutter = {t.x == Overflow::Scroll ? style.scrollbarWidth : 0.0f,
                         t.y == Overflow::Scroll ? style.scrollbarWidth : 0.0f};
    c.contentBoxInset = padding + c.border;
    c.contentBoxInset.bottom += c.scrollbarGutter.y;
    if (c.layoutDirection == Direction::Ltr) {
        c.contentBoxInset.right += c.scrollbarGutter.x;
    } else {
        c.contentBoxInset.left += c.scrollbarGutter.x;
    }

    c.nodeOuterSize = knownDimensions;
    c.nodeInnerSize = MaybeSub(c.nodeOuterSize, c.contentBoxInset.SumAxes());
    c.gap = style.gap
                .ResolveOrZero(c.nodeInnerSize.Or(SizeOptF::New(0, 0)), calc);

    c.minSize = MaybeAdd(style.minSize.MaybeResolve(parentSize, calc)
                             .MaybeApplyAspectRatio(aspectRatio),
                         boxSizingAdjustment);
    c.maxSize = MaybeAdd(style.maxSize.MaybeResolve(parentSize, calc)
                             .MaybeApplyAspectRatio(aspectRatio),
                         boxSizingAdjustment);
    return c;
}

// ─── generate_anonymous_flex_items ───────────────────────────────────────
//
// 9.1 Initial Setup — https://www.w3.org/TR/css-flexbox-1/#algo-anon-box

void GenerateAnonymousFlexItems(TaffyTree* tree, NodeId node,
                                const AlgoConstants& c,
                                Vec<FlexItem>* flexItems) {
    CalcResolver calc = tree->calc;
    int n = tree->ChildCount(node);
    for (int index = 0; index < n; index++) {
        NodeId child = tree->GetChildId(node, index);
        const Style& cs = tree->GetStyle(child);
        if (cs.position == Position::Absolute ||
            cs.BoxGenMode() == BoxGenerationMode::None) {
            continue;
        }

        Optf aspectRatio = cs.aspectRatio;
        RectF padding = cs.padding.ResolveOrZero(c.nodeInnerSize.width, calc);
        RectF border = cs.border.ResolveOrZero(c.nodeInnerSize.width, calc);
        SizeF pbSum = (padding + border).SumAxes();
        SizeF boxSizingAdjustment =
            cs.boxSizing == BoxSizing::ContentBox ? pbSum : SizeF::Zero();

        FlexItem item;
        item.node = child;
        item.order = (uint32_t)index;
        item.size = MaybeAdd(cs.size.MaybeResolve(c.nodeInnerSize, calc)
                                 .MaybeApplyAspectRatio(aspectRatio),
                             boxSizingAdjustment);
        item.minSize = MaybeAdd(cs.minSize.MaybeResolve(c.nodeInnerSize, calc)
                                    .MaybeApplyAspectRatio(aspectRatio),
                                boxSizingAdjustment);
        item.maxSize = MaybeAdd(cs.maxSize.MaybeResolve(c.nodeInnerSize, calc)
                                    .MaybeApplyAspectRatio(aspectRatio),
                                boxSizingAdjustment);
        // The inset resolves left/right against the width and top/bottom
        // against the height, which is Rust's `zip_size`.
        item.inset = cs.inset.MaybeResolveZip(c.nodeInnerSize, calc);
        item.margin = cs.margin.ResolveOrZero(c.nodeInnerSize.width, calc);
        item.marginIsAuto = {cs.margin.left.IsAuto(), cs.margin.right.IsAuto(),
                             cs.margin.top.IsAuto(), cs.margin.bottom.IsAuto()};
        item.padding = padding;
        item.border = border;
        item.alignSelf = cs.alignSelf.UnwrapOr(c.alignItems);
        item.overflow = cs.overflow;
        item.scrollbarWidth = cs.scrollbarWidth;
        item.flexGrow = cs.flexGrow;
        item.flexShrink = cs.flexShrink;
        flexItems->Append(item);
    }
}

// ─── determine_available_space ───────────────────────────────────────────
//
// 9.2 Line Length Determination —
// https://www.w3.org/TR/css-flexbox-1/#algo-available

SizeAvail DetermineAvailableSpace(SizeOptF knownDimensions,
                                  SizeAvail outerAvailableSpace,
                                  const AlgoConstants& c) {
    // min/max/preferred size styles have already been folded into
    // knownDimensions by the caller.
    SizeAvail out;
    if (knownDimensions.width.IsSome()) {
        out.width = AvailableSpace::Definite(
            knownDimensions.width.val - c.contentBoxInset.HorizontalAxisSum());
    } else {
        out.width = MaybeSub(
            MaybeSub(outerAvailableSpace.width, c.margin.HorizontalAxisSum()),
            c.contentBoxInset.HorizontalAxisSum());
    }
    if (knownDimensions.height.IsSome()) {
        out.height = AvailableSpace::Definite(
            knownDimensions.height.val - c.contentBoxInset.VerticalAxisSum());
    } else {
        out.height = MaybeSub(
            MaybeSub(outerAvailableSpace.height, c.margin.VerticalAxisSum()),
            c.contentBoxInset.VerticalAxisSum());
    }
    return out;
}

// ─── determine_flex_base_size ────────────────────────────────────────────
//
// https://www.w3.org/TR/css-flexbox-1/#algo-main-item

void DetermineFlexBaseSize(TaffyTree* tree, const AlgoConstants& c,
                           SizeAvail availableSpace, FlexItem* items,
                           int count) {
    CalcResolver calc = tree->calc;
    FlexDirection dir = c.dir;

    for (int i = 0; i < count; i++) {
        FlexItem& child = items[i];
        const Style& cs = tree->GetStyle(child.node);

        Optf crossAxisParentSize = c.nodeInnerSize.Cross(dir);
        SizeOptF childParentSize =
            SizeOptF::FromCross(dir, crossAxisParentSize);

        float crossAxisMarginSum = c.margin.CrossAxisSum(dir);
        Optf childMinCross =
            MaybeAdd(child.minSize.Cross(dir), crossAxisMarginSum);
        Optf childMaxCross =
            MaybeAdd(child.maxSize.Cross(dir), crossAxisMarginSum);

        // Clamp the available space by the item's min- and max- size.
        AvailableSpace crossAxisAvailableSpace;
        AvailableSpace crossIn = availableSpace.Cross(dir);
        switch (crossIn.kind) {
            case AvailableSpace::Kind::Definite:
                crossAxisAvailableSpace = AvailableSpace::Definite(
                    MaybeClamp(crossAxisParentSize.UnwrapOr(crossIn.value),
                               childMinCross, childMaxCross));
                break;
            case AvailableSpace::Kind::MinContent:
                crossAxisAvailableSpace =
                    childMinCross.IsSome()
                        ? AvailableSpace::Definite(childMinCross.val)
                        : AvailableSpace::MinContent();
                break;
            default:
                crossAxisAvailableSpace =
                    childMaxCross.IsSome()
                        ? AvailableSpace::Definite(childMaxCross.val)
                        : AvailableSpace::MaxContent();
                break;
        }

        SizeOptF childKnownDimensions = child.size;
        childKnownDimensions.SetMain(dir, Optf());
        if (child.alignSelf.keyword == AlignItemsKeyword::Stretch &&
            !child.marginIsAuto.CrossStart(dir) &&
            !child.marginIsAuto.CrossEnd(dir) &&
            !childKnownDimensions.Cross(dir).IsSome()) {
            childKnownDimensions
                .SetCross(dir, MaybeSub(crossAxisAvailableSpace.IntoOption(),
                                        child.margin.CrossAxisSum(dir)));
        }

        Optf containerWidth = c.nodeInnerSize.Main(dir);
        float boxSizingAdjustment = 0.0f;
        if (cs.boxSizing == BoxSizing::ContentBox) {
            RectF padding = cs.padding.ResolveOrZero(containerWidth, calc);
            RectF border = cs.border.ResolveOrZero(containerWidth, calc);
            boxSizingAdjustment = (padding + border).SumAxes().Main(dir);
        }
        Optf flexBasis =
            MaybeAdd(cs.flexBasis.MaybeResolve(containerWidth, calc),
                     boxSizingAdjustment);

        // A. If the item has a definite used flex basis, that is the flex base
        //    size.
        // B. If the item has an intrinsic aspect ratio, a used flex basis of
        //    content and a definite cross size, the base size comes from its
        //    inner cross size and the ratio. `child.size` was already resolved
        //    against the aspect ratio in GenerateAnonymousFlexItems, so using
        //    the main size covers this.
        Optf mainSize = child.size.Main(dir);
        Optf definiteBasis = flexBasis.Or(mainSize);
        if (definiteBasis.IsSome()) {
            child.flexBasis = definiteBasis.val;
        } else {
            // C is covered by E below, which passes the available space
            // constraint through to the child.
            // D would need vertical writing modes.
            // E. Size the item into the available space using its used flex
            //    basis in place of its main size, treating `content` as
            //    max-content.
            SizeAvail childAvailableSpace = SizeAvail::MaxContent();
            childAvailableSpace
                .SetMain(dir, availableSpace.Main(dir).kind ==
                                      AvailableSpace::Kind::MinContent
                                  ? AvailableSpace::MinContent()
                                  : AvailableSpace::MaxContent());
            childAvailableSpace.SetCross(dir, crossAxisAvailableSpace);
            child.flexBasis = tree->MeasureChildSize(
                child.node, childKnownDimensions, childParentSize,
                childAvailableSpace, SizingMode::ContentSize, MainAxis(dir),
                LineBool::False());
        }

        // Floor the flex basis by the padding+border sum, which floors the
        // inner flex basis at zero. The spec says the content box should
        // *not* be floored here, but this matches Chrome and Firefox.
        float paddingBorderSum =
            child.padding.MainAxisSum(dir) + child.border.MainAxisSum(dir);
        child.flexBasis = F32Max(child.flexBasis, paddingBorderSum);

        child.innerFlexBasis = child.flexBasis -
                               child.padding.MainAxisSum(dir) -
                               child.border.MainAxisSum(dir);

        SizeOptF paddingBorderAxesSums =
            AsOptional((child.padding + child.border).SumAxes());

        // The main-axis `parent_size` is deliberately left unset below: it is
        // used to resolve percentages, and a percentage size in an axis must
        // not contribute to a min-content contribution in that same axis. The
        // cross axis keeps its usual values so wrapping content wraps right.
        // https://drafts.csswg.org/css-sizing-3/#min-percentage-contribution
        SizeOptF automaticMin = {MaybeIntoAutomaticMinSize(child.overflow.x),
                                 MaybeIntoAutomaticMinSize(child.overflow.y)};
        Optf styleMinMainSize = child.minSize.Or(automaticMin).Main(dir);

        if (styleMinMainSize.IsSome()) {
            child.resolvedMinimumMainSize = styleMinMainSize.val;
        } else {
            SizeAvail childAvailableSpace = SizeAvail::MinContent();
            childAvailableSpace.SetCross(dir, crossAxisAvailableSpace);
            float minContentMainSize = tree->MeasureChildSize(
                child.node, childKnownDimensions, childParentSize,
                childAvailableSpace, SizingMode::ContentSize, MainAxis(dir),
                LineBool::False());
            // 4.5 Automatic Minimum Size of Flex Items
            // https://www.w3.org/TR/css-flexbox-1/#min-size-auto
            float clamped =
                MaybeMin(MaybeMin(minContentMainSize, child.size.Main(dir)),
                         child.maxSize.Main(dir));
            child.resolvedMinimumMainSize =
                MaybeMax(clamped, paddingBorderAxesSums.Main(dir));
        }

        float hypotheticalInnerMinMain = MaybeMax(
            child.resolvedMinimumMainSize, paddingBorderAxesSums.Main(dir));
        float hypotheticalInnerSize =
            MaybeClamp(child.flexBasis, Optf(hypotheticalInnerMinMain),
                       child.maxSize.Main(dir));
        float hypotheticalOuterSize =
            hypotheticalInnerSize + child.margin.MainAxisSum(dir);

        child.hypotheticalInnerSize.SetMain(dir, hypotheticalInnerSize);
        child.hypotheticalOuterSize.SetMain(dir, hypotheticalOuterSize);
    }
}

// ─── collect_flex_lines ──────────────────────────────────────────────────
//
// https://www.w3.org/TR/css-flexbox-1/#algo-line-break

void CollectFlexLines(const AlgoConstants& c, SizeAvail availableSpace,
                      Vec<FlexItem>* flexItems, Vec<FlexLine>* lines) {
    FlexItem* items = flexItems->els;
    int total = flexItems->len;

    if (!c.isWrap) {
        lines->Append({items, total, 0.0f, 0.0f});
        return;
    }

    AvailableSpace mainAxisAvailableSpace;
    Optf maxMain = c.maxSize.Main(c.dir);
    if (maxMain.IsSome()) {
        mainAxisAvailableSpace = AvailableSpace::Definite(MaybeMax(
            availableSpace.Main(c.dir).IntoOption().UnwrapOr(maxMain.val),
            c.minSize.Main(c.dir)));
    } else {
        mainAxisAvailableSpace = availableSpace.Main(c.dir);
    }

    switch (mainAxisAvailableSpace.kind) {
        case AvailableSpace::Kind::MaxContent:
            // Under a max-content constraint the items never wrap.
            lines->Append({items, total, 0.0f, 0.0f});
            return;
        case AvailableSpace::Kind::MinContent:
            // Under a min-content constraint every wrapping opportunity is
            // taken, so each item gets its own line.
            for (int i = 0; i < total; i++) {
                lines->Append({items + i, 1, 0.0f, 0.0f});
            }
            return;
        default:
            break;
    }

    float limit = mainAxisAvailableSpace.value;
    float mainAxisGap = c.gap.Main(c.dir);
    int start = 0;
    while (start < total) {
        float lineLength = 0.0f;
        int index = total;
        for (int idx = start; idx < total; idx++) {
            // Gaps only occur between items, so the first one in a line does
            // not contribute one.
            float gapContribution = idx == start ? 0.0f : mainAxisGap;
            lineLength +=
                items[idx].hypotheticalOuterSize.Main(c.dir) + gapContribution;
            if (lineLength > limit && idx != start) {
                index = idx;
                break;
            }
        }
        lines->Append({items + start, index - start, 0.0f, 0.0f});
        start = index;
    }
}

// The sum of the items' target sizes on a line, floored per item by its
// padding+border. Used twice by DetermineContainerMainSize.
float LineTotalTargetSize(const FlexLine& line, const AlgoConstants& c) {
    float total = 0.0f;
    for (int i = 0; i < line.count; i++) {
        const FlexItem& child = line.items[i];
        float paddingBorderSum = (child.padding + child.border)
                                     .MainAxisSum(c.dir);
        total += F32Max(MaybeMax(child.flexBasis, child.minSize.Main(c.dir)) +
                            child.margin.MainAxisSum(c.dir),
                        paddingBorderSum);
    }
    return total;
}

float LongestLineLength(Vec<FlexLine>* lines, const AlgoConstants& c) {
    float longest = 0.0f;
    for (int i = 0; i < lines->len; i++) {
        FlexLine& line = (*lines)[i];
        float lineGap = SumAxisGaps(c.gap.Main(c.dir), line.count);
        float total = LineTotalTargetSize(line, c) + lineGap;
        if (i == 0 || total > longest) {
            longest = total;
        }
    }
    return longest;
}

// ─── determine_container_main_size ───────────────────────────────────────

void DetermineContainerMainSize(TaffyTree* tree, SizeAvail availableSpace,
                                Vec<FlexLine>* lines, AlgoConstants* c) {
    FlexDirection dir = c->dir;
    float mainContentBoxInset = c->contentBoxInset.MainAxisSum(dir);

    float outerMainSize;
    Optf known = c->nodeOuterSize.Main(dir);
    if (known.IsSome()) {
        outerMainSize = known.val;
    } else {
        AvailableSpace mainAvail = availableSpace.Main(dir);
        if (mainAvail.kind == AvailableSpace::Kind::Definite) {
            float longest = LongestLineLength(lines, *c);
            float size = longest + mainContentBoxInset;
            outerMainSize =
                lines->len > 1 ? F32Max(size, mainAvail.value) : size;
        } else if (mainAvail.kind == AvailableSpace::Kind::MinContent &&
                   c->isWrap) {
            outerMainSize = LongestLineLength(lines, *c) + mainContentBoxInset;
        } else {
            // The flex container's max-content size is the largest sum of the
            // items' sizes within a single line.
            float mainSize = 0.0f;
            for (int li = 0; li < lines->len; li++) {
                FlexLine& line = (*lines)[li];
                for (int ii = 0; ii < line.count; ii++) {
                    FlexItem& item = line.items[ii];
                    Optf styleMin = item.minSize.Main(dir);
                    Optf stylePreferred = item.size.Main(dir);
                    Optf styleMax = item.maxSize.Main(dir);

                    // The spec reads as though `.maybe_max(style_preferred)`
                    // should not be here, but this matches Chrome and Firefox.
                    // https://www.w3.org/TR/css-flexbox-1/#change-2016-max-contribution
                    Optf clampingBasis =
                        MaybeMax(Optf(item.flexBasis), stylePreferred);
                    Optf flexBasisMin =
                        Filter(clampingBasis, item.flexShrink == 0.0f);
                    Optf flexBasisMax =
                        Filter(clampingBasis, item.flexGrow == 0.0f);

                    float minMainSize =
                        F32Max(MaybeMax(styleMin, flexBasisMin)
                                   .Or(flexBasisMin)
                                   .UnwrapOr(item.resolvedMinimumMainSize),
                               item.resolvedMinimumMainSize);
                    float maxMainSize = MaybeMin(styleMax, flexBasisMax)
                                            .Or(flexBasisMax)
                                            .UnwrapOr(INFINITY);

                    float contentContribution;
                    if (stylePreferred.IsSome() &&
                        (maxMainSize <= minMainSize ||
                         maxMainSize <= stylePreferred.val)) {
                        // The clamping values override the content size, so
                        // computing it would be wasted work.
                        contentContribution =
                            F32Max(F32Min(stylePreferred.val, maxMainSize),
                                   minMainSize) +
                            item.margin.MainAxisSum(dir);
                    } else if (maxMainSize <= minMainSize) {
                        contentContribution =
                            minMainSize + item.margin.MainAxisSum(dir);
                    } else if (item.IsScroll()) {
                        contentContribution =
                            item.flexBasis + item.margin.MainAxisSum(dir);
                    } else {
                        Optf crossAxisParentSize = c->nodeInnerSize.Cross(dir);
                        float crossAxisMarginSum = c->margin.CrossAxisSum(dir);
                        Optf childMinCross = MaybeAdd(item.minSize.Cross(dir),
                                                      crossAxisMarginSum);
                        Optf childMaxCross = MaybeAdd(item.maxSize.Cross(dir),
                                                      crossAxisMarginSum);
                        AvailableSpace crossAxisAvailableSpace =
                            availableSpace.Cross(dir);
                        if (crossAxisAvailableSpace
                                .kind == AvailableSpace::Kind::Definite) {
                            crossAxisAvailableSpace = AvailableSpace::Definite(
                                crossAxisParentSize
                                    .UnwrapOr(crossAxisAvailableSpace.value));
                        }
                        crossAxisAvailableSpace =
                            MaybeClamp(crossAxisAvailableSpace, childMinCross,
                                       childMaxCross);

                        SizeAvail childAvailableSpace = availableSpace;
                        childAvailableSpace
                            .SetCross(dir, crossAxisAvailableSpace);

                        SizeOptF childKnownDimensions = item.size;
                        childKnownDimensions.SetMain(dir, Optf());
                        if (item.alignSelf
                                    .keyword == AlignItemsKeyword::Stretch &&
                            !childKnownDimensions.Cross(dir).IsSome()) {
                            childKnownDimensions.SetCross(
                                dir,
                                MaybeSub(crossAxisAvailableSpace.IntoOption(),
                                         item.margin.CrossAxisSum(dir)));
                        }

                        float contentMainSize =
                            tree->MeasureChildSize(
                                item.node, childKnownDimensions,
                                c->nodeInnerSize, childAvailableSpace,
                                SizingMode::InherentSize, MainAxis(dir),
                                LineBool::False()) +
                            item.margin.MainAxisSum(dir);

                        // Asymmetrical between rows and columns. This likely
                        // relates to
                        // https://drafts.csswg.org/css-flexbox-1/#algo-main-container
                        // — "the automatic block size of a block-level flex
                        // container is its max-content size" — but it was
                        // found by matching Webkit/Firefox output rather than
                        // by reading the spec.
                        if (c->isRow) {
                            contentContribution = F32Max(
                                MaybeClamp(contentMainSize, styleMin, styleMax),
                                mainContentBoxInset);
                        } else {
                            contentContribution = F32Max(
                                MaybeClamp(
                                    F32Max(contentMainSize, item.flexBasis),
                                    styleMin, styleMax),
                                mainContentBoxInset);
                        }
                    }

                    float diff = contentContribution - item.flexBasis;
                    if (diff > 0.0f) {
                        item.contentFlexFraction =
                            diff / F32Max(1.0f, item.flexGrow);
                    } else if (diff < 0.0f) {
                        float scaledShrinkFactor =
                            F32Max(1.0f, item.flexShrink * item.innerFlexBasis);
                        item.contentFlexFraction = diff / scaledShrinkFactor;
                    } else {
                        item.contentFlexFraction = 0.0f;
                    }
                }

                // The spec says to scale everything by the line's max flex
                // fraction, but neither Chrome nor Firefox implements that, so
                // neither do we — each item uses its own fraction.
                //
                // Add each item's flex base size to the product of its flex
                // grow factor (or scaled shrink factor, if the fraction was
                // negative) and that fraction.
                float itemMainSizeSum = 0.0f;
                for (int ii = 0; ii < line.count; ii++) {
                    FlexItem& item = line.items[ii];
                    float flexFraction = item.contentFlexFraction;
                    float flexContribution = 0.0f;
                    if (item.contentFlexFraction > 0.0f) {
                        flexContribution =
                            F32Max(1.0f, item.flexGrow) * flexFraction;
                    } else if (item.contentFlexFraction < 0.0f) {
                        float scaledShrinkFactor =
                            F32Max(1.0f, item.flexShrink) * item.innerFlexBasis;
                        flexContribution = scaledShrinkFactor * flexFraction;
                    }
                    float size = item.flexBasis + flexContribution;
                    item.outerTargetSize.SetMain(dir, size);
                    item.targetSize.SetMain(dir, size);
                    itemMainSizeSum += size;
                }

                float gapSum = SumAxisGaps(c->gap.Main(dir), line.count);
                mainSize = F32Max(mainSize, itemMainSizeSum + gapSum);
            }
            outerMainSize = mainSize + mainContentBoxInset;
        }
    }

    outerMainSize = F32Max(
        MaybeClamp(outerMainSize, c->minSize.Main(dir), c->maxSize.Main(dir)),
        mainContentBoxInset - c->scrollbarGutter.Main(dir));
    float innerMainSize = F32Max(outerMainSize - mainContentBoxInset, 0.0f);
    c->containerSize.SetMain(dir, outerMainSize);
    c->innerContainerSize.SetMain(dir, innerMainSize);
    c->nodeInnerSize.SetMain(dir, Optf(innerMainSize));
}

// ─── resolve_flexible_lengths ────────────────────────────────────────────
//
// 9.7 Resolving Flexible Lengths —
// https://www.w3.org/TR/css-flexbox-1/#resolve-flexible-lengths

void ResolveFlexibleLengths(FlexLine* line, const AlgoConstants& c) {
    FlexDirection dir = c.dir;
    float totalMainAxisGap = SumAxisGaps(c.gap.Main(dir), line->count);

    // 1. Determine the used flex factor.
    float totalHypotheticalOuterMainSize = 0.0f;
    for (int i = 0; i < line->count; i++) {
        totalHypotheticalOuterMainSize += line->items[i]
                                              .hypotheticalOuterSize.Main(dir);
    }
    float usedFlexFactor = totalMainAxisGap + totalHypotheticalOuterMainSize;
    float innerMain = c.nodeInnerSize.Main(dir).UnwrapOr(0.0f);
    bool growing = usedFlexFactor < innerMain;
    bool shrinking = usedFlexFactor > innerMain;
    bool exactlySized = !growing && !shrinking;

    // 2. Size inflexible items: freeze, setting the target main size to the
    //    hypothetical main size.
    for (int i = 0; i < line->count; i++) {
        FlexItem& child = line->items[i];
        float innerTargetSize = child.hypotheticalInnerSize.Main(dir);
        child.targetSize.SetMain(dir, innerTargetSize);

        if (exactlySized ||
            (child.flexGrow == 0.0f && child.flexShrink == 0.0f) ||
            (growing && child.flexBasis > innerTargetSize) ||
            (shrinking && child.flexBasis < innerTargetSize)) {
            child.frozen = true;
            child.outerTargetSize
                .SetMain(dir, innerTargetSize + child.margin.MainAxisSum(dir));
        }
    }

    if (exactlySized) {
        return;
    }

    // 3. Calculate the initial free space.
    float usedSpace = totalMainAxisGap;
    for (int i = 0; i < line->count; i++) {
        FlexItem& child = line->items[i];
        usedSpace += child.frozen
                         ? child.outerTargetSize.Main(dir)
                         : child.flexBasis + child.margin.MainAxisSum(dir);
    }
    float initialFreeSpace = MaybeSub(c.nodeInnerSize.Main(dir), usedSpace)
                                 .UnwrapOr(0.0f);

    // 4. Loop.
    while (true) {
        // a. If every item on the line is frozen, the free space has been
        //    distributed.
        bool allFrozen = true;
        for (int i = 0; i < line->count; i++) {
            if (!line->items[i].frozen) {
                allFrozen = false;
                break;
            }
        }
        if (allFrozen) {
            break;
        }

        // b. Recalculate the remaining free space. If the unfrozen items' flex
        //    factors sum to less than one, scale the initial free space by
        //    that sum and use it when it is the smaller magnitude.
        usedSpace = totalMainAxisGap;
        float sumFlexGrow = 0.0f;
        float sumFlexShrink = 0.0f;
        for (int i = 0; i < line->count; i++) {
            FlexItem& child = line->items[i];
            usedSpace += child.frozen
                             ? child.outerTargetSize.Main(dir)
                             : child.flexBasis + child.margin.MainAxisSum(dir);
            if (!child.frozen) {
                sumFlexGrow += child.flexGrow;
                sumFlexShrink += child.flexShrink;
            }
        }

        Optf remaining = MaybeSub(c.nodeInnerSize.Main(dir), usedSpace);
        float freeSpace;
        if (growing && sumFlexGrow < 1.0f) {
            freeSpace = MaybeMin(
                initialFreeSpace * sumFlexGrow - totalMainAxisGap, remaining);
        } else if (shrinking && sumFlexShrink < 1.0f) {
            freeSpace = MaybeMax(
                initialFreeSpace * sumFlexShrink - totalMainAxisGap, remaining);
        } else {
            freeSpace = remaining.UnwrapOr(usedFlexFactor - usedSpace);
        }

        // c. Distribute the free space proportionally to the flex factors.
        //    Rust guards this with `f32::is_normal`, which excludes zero,
        //    subnormals, infinities and NaN.
        bool isNormal = std::isnormal(freeSpace);
        if (isNormal) {
            if (growing && sumFlexGrow > 0.0f) {
                for (int i = 0; i < line->count; i++) {
                    FlexItem& child = line->items[i];
                    if (child.frozen) {
                        continue;
                    }
                    child.targetSize.SetMain(
                        dir, child.flexBasis +
                                 freeSpace * (child.flexGrow / sumFlexGrow));
                }
            } else if (shrinking && sumFlexShrink > 0.0f) {
                float sumScaledShrinkFactor = 0.0f;
                for (int i = 0; i < line->count; i++) {
                    FlexItem& child = line->items[i];
                    if (!child.frozen) {
                        sumScaledShrinkFactor +=
                            child.innerFlexBasis * child.flexShrink;
                    }
                }
                if (sumScaledShrinkFactor > 0.0f) {
                    for (int i = 0; i < line->count; i++) {
                        FlexItem& child = line->items[i];
                        if (child.frozen) {
                            continue;
                        }
                        float scaledShrinkFactor =
                            child.innerFlexBasis * child.flexShrink;
                        child.targetSize.SetMain(
                            dir, child.flexBasis +
                                     freeSpace * (scaledShrinkFactor /
                                                  sumScaledShrinkFactor));
                    }
                }
            }
        }

        // d. Fix min/max violations: clamp each unfrozen item's target main
        //    size and floor its content box at zero.
        float totalViolation = 0.0f;
        for (int i = 0; i < line->count; i++) {
            FlexItem& child = line->items[i];
            if (child.frozen) {
                continue;
            }
            Optf resolvedMinMain = Optf(child.resolvedMinimumMainSize);
            Optf maxMain = child.maxSize.Main(dir);
            float clamped = F32Max(MaybeClamp(child.targetSize.Main(dir),
                                              resolvedMinMain, maxMain),
                                   0.0f);
            child.violation = clamped - child.targetSize.Main(dir);
            child.targetSize.SetMain(dir, clamped);
            child.outerTargetSize
                .SetMain(dir, clamped + child.margin.MainAxisSum(dir));
            totalViolation += child.violation;
        }

        // e. Freeze over-flexed items: all of them if the total violation is
        //    zero, the min-violating ones if it is positive, the
        //    max-violating ones if it is negative.
        for (int i = 0; i < line->count; i++) {
            FlexItem& child = line->items[i];
            if (child.frozen) {
                continue;
            }
            if (totalViolation > 0.0f) {
                child.frozen = child.violation > 0.0f;
            } else if (totalViolation < 0.0f) {
                child.frozen = child.violation < 0.0f;
            } else {
                child.frozen = true;
            }
        }
    }
}

// ─── determine_hypothetical_cross_size ───────────────────────────────────
//
// https://www.w3.org/TR/css-flexbox-1/#algo-cross-item

void DetermineHypotheticalCrossSize(TaffyTree* tree, FlexLine* line,
                                    const AlgoConstants& c,
                                    SizeAvail availableSpace) {
    FlexDirection dir = c.dir;
    for (int i = 0; i < line->count; i++) {
        FlexItem& child = line->items[i];
        float paddingBorderSum = (child.padding + child.border)
                                     .CrossAxisSum(dir);

        AvailableSpace childKnownMain =
            AvailableSpace::Definite(c.containerSize.Main(dir));

        Optf childCross =
            MaybeMax(MaybeClamp(child.size.Cross(dir), child.minSize.Cross(dir),
                                child.maxSize.Cross(dir)),
                     paddingBorderSum);

        AvailableSpace childAvailableCross = MaybeMax(
            MaybeClamp(availableSpace.Cross(dir), child.minSize.Cross(dir),
                       child.maxSize.Cross(dir)),
            paddingBorderSum);

        float childInnerCross;
        if (childCross.IsSome()) {
            childInnerCross = childCross.val;
        } else {
            SizeOptF known = {
                c.isRow ? Optf(child.targetSize.width) : childCross,
                c.isRow ? childCross : Optf(child.targetSize.height)};
            SizeAvail avail = {c.isRow ? childKnownMain : childAvailableCross,
                               c.isRow ? childAvailableCross : childKnownMain};
            float measured = tree->MeasureChildSize(
                child.node, known, c.nodeInnerSize, avail,
                SizingMode::ContentSize, CrossAxis(dir), LineBool::False());
            childInnerCross =
                F32Max(MaybeClamp(measured, child.minSize.Cross(dir),
                                  child.maxSize.Cross(dir)),
                       paddingBorderSum);
        }
        float childOuterCross = childInnerCross + child.margin
                                                      .CrossAxisSum(dir);

        child.hypotheticalInnerSize.SetCross(dir, childInnerCross);
        child.hypotheticalOuterSize.SetCross(dir, childOuterCross);
    }
}

// ─── calculate_children_base_lines ───────────────────────────────────────

void CalculateChildrenBaseLines(TaffyTree* tree, SizeOptF nodeSize,
                                SizeAvail availableSpace, Vec<FlexLine>* lines,
                                const AlgoConstants& c) {
    // Baselines are only computed for flex rows: baseline alignment is only
    // supported in the cross axis where that axis is also the inline axis.
    if (!c.isRow) {
        return;
    }

    for (int li = 0; li < lines->len; li++) {
        FlexLine& line = (*lines)[li];
        // Baseline alignment is a no-op on a line with one or zero
        // participating items.
        int participating = 0;
        for (int i = 0; i < line.count; i++) {
            if (line.items[i]
                    .alignSelf.keyword == AlignItemsKeyword::Baseline) {
                participating++;
            }
        }
        if (participating <= 1) {
            continue;
        }

        for (int i = 0; i < line.count; i++) {
            FlexItem& child = line.items[i];
            if (child.alignSelf.keyword != AlignItemsKeyword::Baseline) {
                continue;
            }
            SizeOptF known = {c.isRow ? Optf(child.targetSize.width)
                                      : Optf(child.hypotheticalInnerSize.width),
                              c.isRow ? Optf(child.hypotheticalInnerSize.height)
                                      : Optf(child.targetSize.height)};
            SizeAvail avail = {
                c.isRow ? AvailableSpace::Definite(c.containerSize.width)
                        : availableSpace.width.MaybeSet(nodeSize.width),
                c.isRow ? availableSpace.height.MaybeSet(nodeSize.height)
                        : AvailableSpace::Definite(c.containerSize.height)};
            LayoutOutput out = tree->PerformChildLayout(
                child.node, known, c.nodeInnerSize, avail,
                SizingMode::ContentSize, LineBool::False());
            child.baseline = out.firstBaselines.y.UnwrapOr(out.size.height) +
                             child.margin.top;
        }
    }
}

// ─── calculate_cross_size ────────────────────────────────────────────────
//
// https://www.w3.org/TR/css-flexbox-1/#algo-cross-line

void CalculateCrossSize(Vec<FlexLine>* lines, SizeOptF nodeSize,
                        const AlgoConstants& c) {
    FlexDirection dir = c.dir;
    if (lines->len == 0) {
        return;
    }
    // A single-line container with a definite cross size gives its line the
    // container's inner cross size.
    if (!c.isWrap && nodeSize.Cross(dir).IsSome()) {
        float crossAxisPaddingBorder = c.contentBoxInset.CrossAxisSum(dir);
        (*lines)[0].crossSize =
            MaybeMax(
                MaybeSub(MaybeClamp(nodeSize.Cross(dir), c.minSize.Cross(dir),
                                    c.maxSize.Cross(dir)),
                         crossAxisPaddingBorder),
                0.0f)
                .UnwrapOr(0.0f);
        return;
    }

    // Otherwise, for each line: take the largest of the baseline-aligned
    // items' summed distances and of the other items' outer hypothetical
    // cross sizes, and zero.
    for (int li = 0; li < lines->len; li++) {
        FlexLine& line = (*lines)[li];
        float maxBaseline = 0.0f;
        for (int i = 0; i < line.count; i++) {
            maxBaseline = F32Max(maxBaseline, line.items[i].baseline);
        }
        float crossSize = 0.0f;
        for (int i = 0; i < line.count; i++) {
            FlexItem& child = line.items[i];
            float v;
            if (child.alignSelf.keyword == AlignItemsKeyword::Baseline &&
                !child.marginIsAuto.CrossStart(dir) &&
                !child.marginIsAuto.CrossEnd(dir)) {
                v = maxBaseline - child.baseline +
                    child.hypotheticalOuterSize.Cross(dir);
            } else {
                v = child.hypotheticalOuterSize.Cross(dir);
            }
            crossSize = F32Max(crossSize, v);
        }
        line.crossSize = crossSize;
    }

    // A single-line container clamps its line's cross size to its own min and
    // max cross sizes.
    if (!c.isWrap) {
        float crossAxisPaddingBorder = c.contentBoxInset.CrossAxisSum(dir);
        (*lines)[0].crossSize =
            MaybeClamp((*lines)[0].crossSize,
                       MaybeSub(c.minSize.Cross(dir), crossAxisPaddingBorder),
                       MaybeSub(c.maxSize.Cross(dir), crossAxisPaddingBorder));
    }
}

// ─── handle_align_content_stretch ────────────────────────────────────────
//
// https://www.w3.org/TR/css-flexbox-1/#algo-line-stretch

void HandleAlignContentStretch(Vec<FlexLine>* lines, SizeOptF nodeSize,
                               const AlgoConstants& c) {
    if (c.alignContent.keyword != AlignContentKeyword::Stretch ||
        lines->len == 0) {
        return;
    }
    FlexDirection dir = c.dir;
    float crossAxisPaddingBorder = c.contentBoxInset.CrossAxisSum(dir);
    Optf crossMinSize = c.minSize.Cross(dir);
    Optf crossMaxSize = c.maxSize.Cross(dir);
    float containerMinInnerCross =
        MaybeMax(MaybeSub(MaybeClamp(nodeSize.Cross(dir).Or(crossMinSize),
                                     crossMinSize, crossMaxSize),
                          crossAxisPaddingBorder),
                 0.0f)
            .UnwrapOr(0.0f);

    float totalCrossAxisGap = SumAxisGaps(c.gap.Cross(dir), lines->len);
    float linesTotalCross = totalCrossAxisGap;
    for (int i = 0; i < lines->len; i++) {
        linesTotalCross += (*lines)[i].crossSize;
    }

    if (linesTotalCross < containerMinInnerCross) {
        float addition =
            (containerMinInnerCross - linesTotalCross) / (float)lines->len;
        for (int i = 0; i < lines->len; i++) {
            (*lines)[i].crossSize += addition;
        }
    }
}

// ─── determine_used_cross_size ───────────────────────────────────────────
//
// https://www.w3.org/TR/css-flexbox-1/#algo-stretch

void DetermineUsedCrossSize(TaffyTree* tree, Vec<FlexLine>* lines,
                            const AlgoConstants& c) {
    CalcResolver calc = tree->calc;
    FlexDirection dir = c.dir;
    for (int li = 0; li < lines->len; li++) {
        FlexLine& line = (*lines)[li];
        float lineCrossSize = line.crossSize;

        for (int i = 0; i < line.count; i++) {
            FlexItem& child = line.items[i];
            const Style& cs = tree->GetStyle(child.node);
            float used;
            if (child.alignSelf.keyword == AlignItemsKeyword::Stretch &&
                !child.marginIsAuto.CrossStart(dir) &&
                !child.marginIsAuto.CrossEnd(dir) &&
                cs.size.Cross(dir).IsAuto()) {
                // This use of max_size is an exception to the rule that
                // max_size transfers through the aspect ratio. Chrome and
                // Firefox agree, and it is a reasonable reading of the spec.
                RectF padding = cs.padding.ResolveOrZero(c.nodeInnerSize, calc);
                RectF border = cs.border.ResolveOrZero(c.nodeInnerSize, calc);
                SizeF pbSum = (padding + border).SumAxes();
                SizeF boxSizingAdjustment =
                    cs.boxSizing == BoxSizing::ContentBox ? pbSum
                                                          : SizeF::Zero();
                SizeOptF maxSizeIgnoringAspectRatio =
                    MaybeAdd(cs.maxSize.MaybeResolve(c.nodeInnerSize, calc),
                             boxSizingAdjustment);
                used =
                    MaybeClamp(lineCrossSize - child.margin.CrossAxisSum(dir),
                               child.minSize.Cross(dir),
                               maxSizeIgnoringAspectRatio.Cross(dir));
            } else {
                used = child.hypotheticalInnerSize.Cross(dir);
            }
            child.targetSize.SetCross(dir, used);
            child.outerTargetSize
                .SetCross(dir, used + child.margin.CrossAxisSum(dir));
        }
    }
}

// ─── distribute_remaining_free_space ─────────────────────────────────────
//
// https://www.w3.org/TR/css-flexbox-1/#algo-main-align

void DistributeRemainingFreeSpace(Vec<FlexLine>* lines,
                                  const AlgoConstants& c) {
    FlexDirection dir = c.dir;
    for (int li = 0; li < lines->len; li++) {
        FlexLine& line = (*lines)[li];
        float totalMainAxisGap = SumAxisGaps(c.gap.Main(dir), line.count);
        float usedSpace = totalMainAxisGap;
        for (int i = 0; i < line.count; i++) {
            usedSpace += line.items[i].outerTargetSize.Main(dir);
        }
        float freeSpace = c.innerContainerSize.Main(dir) - usedSpace;

        int numAutoMargins = 0;
        for (int i = 0; i < line.count; i++) {
            FlexItem& child = line.items[i];
            if (child.marginIsAuto.MainStart(dir)) {
                numAutoMargins++;
            }
            if (child.marginIsAuto.MainEnd(dir)) {
                numAutoMargins++;
            }
        }

        // 1. Positive free space and at least one auto main margin: split the
        //    free space equally among them.
        if (freeSpace > 0.0f && numAutoMargins > 0) {
            float margin = freeSpace / (float)numAutoMargins;
            for (int i = 0; i < line.count; i++) {
                FlexItem& child = line.items[i];
                if (child.marginIsAuto.MainStart(dir)) {
                    if (c.isRow) {
                        child.margin.left = margin;
                    } else {
                        child.margin.top = margin;
                    }
                }
                if (child.marginIsAuto.MainEnd(dir)) {
                    if (c.isRow) {
                        child.margin.right = margin;
                    } else {
                        child.margin.bottom = margin;
                    }
                }
            }
        }

        // 2. Align the items along the main axis per justify-content.
        int numItems = line.count;
        bool layoutReverse = IsReverse(dir);
        float gap = c.gap.Main(dir);
        JustifyContent rawMode =
            c.justifyContent
                .UnwrapOr(AlignContent{AlignContentKeyword::FlexStart});
        AlignContentKeyword mode =
            ApplyAlignmentFallback(freeSpace, numItems, rawMode);

        for (int i = 0; i < numItems; i++) {
            FlexItem& child =
                layoutReverse ? line.items[numItems - 1 - i] : line.items[i];
            child.offsetMain = ComputeAlignmentOffset(
                freeSpace, numItems, gap, mode, layoutReverse, i == 0);
        }
    }
}

// ─── align_flex_items_along_cross_axis ───────────────────────────────────
//
// https://www.w3.org/TR/css-flexbox-1/#algo-cross-align

float AlignFlexItemsAlongCrossAxis(const FlexItem& child, float freeSpace,
                                   float maxBaseline, const AlgoConstants& c) {
    bool crossAxisShouldReverse =
        c.isColumn && c.layoutDirection == Direction::Rtl;

    // A `safe` align-self whose item would overflow the line falls back to
    // logical Start, per CSS Box Alignment 3 §4.3. Otherwise the safety field
    // is dropped and the switch below sees a bare keyword.
    AlignItemsKeyword keyword = (child.alignSelf.IsSafe() && freeSpace < 0.0f)
                                    ? AlignItemsKeyword::Start
                                    : child.alignSelf.keyword;

    switch (keyword) {
        case AlignItemsKeyword::Start:
            return crossAxisShouldReverse ? freeSpace : 0.0f;
        case AlignItemsKeyword::FlexStart:
            return (c.isWrapReverse != crossAxisShouldReverse) ? freeSpace
                                                               : 0.0f;
        case AlignItemsKeyword::End:
            return crossAxisShouldReverse ? 0.0f : freeSpace;
        case AlignItemsKeyword::FlexEnd:
            return (c.isWrapReverse != crossAxisShouldReverse) ? 0.0f
                                                               : freeSpace;
        case AlignItemsKeyword::Center:
            return freeSpace / 2.0f;
        case AlignItemsKeyword::Baseline: {
            if (c.isRow) {
                return maxBaseline - child.baseline;
            }
            // Without vertical writing modes, baseline alignment only makes
            // sense in a row, so a column treats it as flex-start.
            bool baselineColumnShouldReverse =
                crossAxisShouldReverse && !c.isWrap;
            return (c.isWrapReverse != baselineColumnShouldReverse) ? freeSpace
                                                                    : 0.0f;
        }
        default: // Stretch
            return (c.isWrapReverse != crossAxisShouldReverse) ? freeSpace
                                                               : 0.0f;
    }
}

// ─── resolve_cross_axis_auto_margins ─────────────────────────────────────
//
// https://www.w3.org/TR/css-flexbox-1/#algo-cross-margins

void ResolveCrossAxisAutoMargins(Vec<FlexLine>* lines, const AlgoConstants& c) {
    FlexDirection dir = c.dir;
    for (int li = 0; li < lines->len; li++) {
        FlexLine& line = (*lines)[li];
        float lineCrossSize = line.crossSize;
        float maxBaseline = 0.0f;
        for (int i = 0; i < line.count; i++) {
            maxBaseline = F32Max(maxBaseline, line.items[i].baseline);
        }

        for (int i = 0; i < line.count; i++) {
            FlexItem& child = line.items[i];
            float freeSpace = lineCrossSize - child.outerTargetSize.Cross(dir);

            if (child.marginIsAuto.CrossStart(dir) && child.marginIsAuto
                                                          .CrossEnd(dir)) {
                if (c.isRow) {
                    child.margin.top = freeSpace / 2.0f;
                    child.margin.bottom = freeSpace / 2.0f;
                } else {
                    child.margin.left = freeSpace / 2.0f;
                    child.margin.right = freeSpace / 2.0f;
                }
            } else if (child.marginIsAuto.CrossStart(dir)) {
                if (c.isRow) {
                    child.margin.top = freeSpace;
                } else {
                    child.margin.left = freeSpace;
                }
            } else if (child.marginIsAuto.CrossEnd(dir)) {
                if (c.isRow) {
                    child.margin.bottom = freeSpace;
                } else {
                    child.margin.right = freeSpace;
                }
            } else {
                // 14. Align all flex items along the cross axis.
                child.offsetCross = AlignFlexItemsAlongCrossAxis(
                    child, freeSpace, maxBaseline, c);
            }
        }
    }
}

// ─── determine_container_cross_size ──────────────────────────────────────
//
// https://www.w3.org/TR/css-flexbox-1/#algo-cross-container

float DetermineContainerCrossSize(Vec<FlexLine>* lines, SizeOptF nodeSize,
                                  AlgoConstants* c) {
    FlexDirection dir = c->dir;
    float totalCrossAxisGap = SumAxisGaps(c->gap.Cross(dir), lines->len);
    float totalLineCrossSize = 0.0f;
    for (int i = 0; i < lines->len; i++) {
        totalLineCrossSize += (*lines)[i].crossSize;
    }

    float paddingBorderSum = c->contentBoxInset.CrossAxisSum(dir);
    float crossScrollbarGutter = c->scrollbarGutter.Cross(dir);
    float outerContainerSize =
        F32Max(MaybeClamp(nodeSize.Cross(dir)
                              .UnwrapOr(totalLineCrossSize + totalCrossAxisGap +
                                        paddingBorderSum),
                          c->minSize.Cross(dir), c->maxSize.Cross(dir)),
               paddingBorderSum - crossScrollbarGutter);
    float innerContainerSize =
        F32Max(outerContainerSize - paddingBorderSum, 0.0f);

    c->containerSize.SetCross(dir, outerContainerSize);
    c->innerContainerSize.SetCross(dir, innerContainerSize);

    return totalLineCrossSize;
}

// ─── align_flex_lines_per_align_content ──────────────────────────────────
//
// https://www.w3.org/TR/css-flexbox-1/#algo-line-align

void AlignFlexLinesPerAlignContent(Vec<FlexLine>* lines, const AlgoConstants& c,
                                   float totalCrossSize) {
    int numLines = lines->len;
    float gap = c.gap.Cross(c.dir);
    float totalCrossAxisGap = SumAxisGaps(gap, numLines);
    float freeSpace =
        c.innerContainerSize.Cross(c.dir) - totalCrossSize - totalCrossAxisGap;

    AlignContentKeyword mode =
        ApplyAlignmentFallback(freeSpace, numLines, c.alignContent);

    for (int i = 0; i < numLines; i++) {
        FlexLine& line =
            c.isWrapReverse ? (*lines)[numLines - 1 - i] : (*lines)[i];
        line.offsetCross = ComputeAlignmentOffset(
            freeSpace, numLines, gap, mode, c.isWrapReverse, i == 0);
    }
}

// ─── calculate_flex_item ─────────────────────────────────────────────────

void CalculateFlexItem(TaffyTree* tree, FlexItem* item, float* totalOffsetMain,
                       float totalOffsetCross, float lineOffsetCross,
                       SizeF* totalContentSize, SizeF containerSize,
                       SizeOptF nodeInnerSize, FlexDirection direction,
                       Direction layoutDirection) {
    LayoutOutput layoutOutput = tree->PerformChildLayout(
        item->node, AsOptional(item->targetSize), nodeInnerSize,
        SizeAvail::Definite(containerSize), SizingMode::ContentSize,
        LineBool::False());
    SizeF size = layoutOutput.size;
    SizeF contentSize = layoutOutput.contentSize;

    bool isRtlRow = IsRow(direction) && IsRtl(layoutDirection);
    bool isRtlColumn = IsColumn(direction) && IsRtl(layoutDirection);

    Optf negMainStart = item->inset.MainStart(direction);
    if (negMainStart.IsSome()) {
        negMainStart.val = -negMainStart.val;
    }
    Optf negMainEnd = item->inset.MainEnd(direction);
    if (negMainEnd.IsSome()) {
        negMainEnd.val = -negMainEnd.val;
    }
    float mainRelativeInset =
        isRtlRow
            ? item->inset.MainEnd(direction).Or(negMainStart).UnwrapOr(0.0f)
            : item->inset.MainStart(direction).Or(negMainEnd).UnwrapOr(0.0f);

    Optf negCrossEnd = item->inset.CrossEnd(direction);
    if (negCrossEnd.IsSome()) {
        negCrossEnd.val = -negCrossEnd.val;
    }
    float crossRelativeInset =
        isRtlColumn
            ? negCrossEnd.Or(item->inset.CrossStart(direction)).UnwrapOr(0.0f)
            : item->inset.CrossStart(direction).Or(negCrossEnd).UnwrapOr(0.0f);

    float effectiveLineOffsetCross = isRtlColumn ? 0.0f : lineOffsetCross;

    float offsetMain = isRtlRow ? *totalOffsetMain - item->offsetMain -
                                      item->margin.MainEnd(direction) -
                                      mainRelativeInset - size.width
                                : *totalOffsetMain + item->offsetMain +
                                      item->margin.MainStart(direction) +
                                      mainRelativeInset;

    float offsetCross = totalOffsetCross + item->offsetCross +
                        effectiveLineOffsetCross +
                        item->margin.CrossStart(direction) + crossRelativeInset;

    float innerBaseline = layoutOutput.firstBaselines.y.UnwrapOr(size.height);
    if (IsRow(direction)) {
        float baselineOffsetCross = totalOffsetCross + item->offsetCross +
                                    effectiveLineOffsetCross +
                                    item->margin.CrossStart(direction);
        item->baseline = baselineOffsetCross + innerBaseline;
    } else {
        float baselineOffsetMain = *totalOffsetMain + item->offsetMain +
                                   item->margin.MainStart(direction);
        item->baseline = baselineOffsetMain + innerBaseline;
    }

    PointF location = IsRow(direction) ? PointF{offsetMain, offsetCross}
                                       : PointF{offsetCross, offsetMain};
    SizeF scrollbarSize = {
        item->overflow.y == Overflow::Scroll ? item->scrollbarWidth : 0.0f,
        item->overflow.x == Overflow::Scroll ? item->scrollbarWidth : 0.0f};

    Layout layout;
    layout.order = item->order;
    layout.size = size;
    layout.contentSize = contentSize;
    layout.scrollbarSize = scrollbarSize;
    layout.location = location;
    layout.padding = item->padding;
    layout.border = item->border;
    layout.margin = item->margin;
    tree->SetUnroundedLayout(item->node, layout);

    float advance = item->offsetMain + item->margin.MainAxisSum(direction) +
                    size.Main(direction);
    if (isRtlRow) {
        *totalOffsetMain -= advance;
    } else {
        *totalOffsetMain += advance;
    }

    PointF contributionLocation =
        IsRtl(layoutDirection)
            ? PointF{containerSize.width - (location.x + size.width), location
                                                                          .y}
            : location;
    *totalContentSize = totalContentSize->Max(ComputeContentSizeContribution(
        contributionLocation, size, contentSize, item->overflow));
}

// ─── calculate_layout_line ───────────────────────────────────────────────

void CalculateLayoutLine(TaffyTree* tree, FlexLine* line,
                         float* totalOffsetCross, SizeF* contentSize,
                         SizeF containerSize, SizeOptF nodeInnerSize,
                         RectF paddingBorder, FlexDirection direction,
                         Direction layoutDirection) {
    float totalOffsetMain = (IsRtl(layoutDirection) && IsRow(direction))
                                ? containerSize.width - paddingBorder
                                                            .MainEnd(direction)
                                : paddingBorder.MainStart(direction);
    float lineOffsetCross = line->offsetCross;

    bool isRtlColumn = IsRtl(layoutDirection) && IsColumn(direction);
    if (isRtlColumn) {
        *totalOffsetCross -= lineOffsetCross + line->crossSize;
    }

    for (int i = 0; i < line->count; i++) {
        FlexItem* item = IsReverse(direction)
                             ? &line->items[line->count - 1 - i]
                             : &line->items[i];
        CalculateFlexItem(tree, item, &totalOffsetMain, *totalOffsetCross,
                          lineOffsetCross, contentSize, containerSize,
                          nodeInnerSize, direction, layoutDirection);
    }

    if (!isRtlColumn) {
        *totalOffsetCross += lineOffsetCross + line->crossSize;
    }
}

// ─── final_layout_pass ───────────────────────────────────────────────────

SizeF FinalLayoutPass(TaffyTree* tree, Vec<FlexLine>* lines,
                      const AlgoConstants& c) {
    float totalOffsetCross = (c.isColumn && IsRtl(c.layoutDirection))
                                 ? c.containerSize.width - c.contentBoxInset
                                                               .CrossEnd(c.dir)
                                 : c.contentBoxInset.CrossStart(c.dir);

    SizeF contentSize = SizeF::Zero();

    for (int i = 0; i < lines->len; i++) {
        FlexLine& line =
            c.isWrapReverse ? (*lines)[lines->len - 1 - i] : (*lines)[i];
        CalculateLayoutLine(tree, &line, &totalOffsetCross, &contentSize,
                            c.containerSize, c.nodeInnerSize, c.contentBoxInset,
                            c.dir, c.layoutDirection);
    }

    contentSize.width +=
        IsRtl(c.layoutDirection)
            ? c.contentBoxInset.left - c.border.left - c.scrollbarGutter.x
            : c.contentBoxInset.right - c.border.right - c.scrollbarGutter.x;
    contentSize.height +=
        c.contentBoxInset.bottom - c.border.bottom - c.scrollbarGutter.y;

    return contentSize;
}

// ─── perform_absolute_layout_on_absolute_children ────────────────────────

SizeF PerformAbsoluteLayoutOnAbsoluteChildren(TaffyTree* tree, NodeId node,
                                              const AlgoConstants& c) {
    CalcResolver calc = tree->calc;
    float containerWidth = c.containerSize.width;
    float containerHeight = c.containerSize.height;
    SizeF insetRelativeSize =
        c.containerSize - c.border.SumAxes() - c.scrollbarGutter.IntoSize();

    SizeF contentSize = SizeF::Zero();

    int n = tree->ChildCount(node);
    for (int order = 0; order < n; order++) {
        NodeId child = tree->GetChildId(node, order);
        const Style& cs = tree->GetStyle(child);

        if (cs.BoxGenMode() == BoxGenerationMode::None ||
            cs.position != Position::Absolute) {
            continue;
        }

        PointOverflow overflow = cs.overflow;
        float scrollbarWidth = cs.scrollbarWidth;
        Optf aspectRatio = cs.aspectRatio;
        AlignSelf alignSelf = cs.alignSelf.UnwrapOr(c.alignItems);
        RectOptF margin =
            cs.margin.MaybeResolve(Optf(insetRelativeSize.width), calc);
        RectF padding = cs.padding
                            .ResolveOrZero(Optf(insetRelativeSize.width), calc);
        RectF border = cs.border
                           .ResolveOrZero(Optf(insetRelativeSize.width), calc);
        SizeF paddingBorderSum = (padding + border).SumAxes();
        SizeF boxSizingAdjustment = cs.boxSizing == BoxSizing::ContentBox
                                        ? paddingBorderSum
                                        : SizeF::Zero();

        // Insets resolve against the container size minus its border.
        RectOptF inset =
            cs.inset.MaybeResolveZip(AsOptional(insetRelativeSize), calc);
        Optf left = inset.left;
        Optf right = inset.right;
        Optf top = inset.top;
        Optf bottom = inset.bottom;

        SizeOptF styleSize =
            MaybeAdd(cs.size.MaybeResolve(AsOptional(insetRelativeSize), calc)
                         .MaybeApplyAspectRatio(aspectRatio),
                     boxSizingAdjustment);
        SizeOptF minSize = MaybeMax(
            MaybeAdd(cs.minSize
                         .MaybeResolve(AsOptional(insetRelativeSize), calc)
                         .MaybeApplyAspectRatio(aspectRatio),
                     boxSizingAdjustment)
                .Or(AsOptional(paddingBorderSum)),
            paddingBorderSum);
        SizeOptF maxSize = MaybeAdd(
            cs.maxSize.MaybeResolve(AsOptional(insetRelativeSize), calc)
                .MaybeApplyAspectRatio(aspectRatio),
            boxSizingAdjustment);
        SizeOptF knownDimensions = MaybeClamp(styleSize, minSize, maxSize);

        // Fill in the width from left/right (and reapply the aspect ratio) if
        // the width is not known and both insets are set.
        if (!knownDimensions.width.IsSome() && left.IsSome() &&
            right.IsSome()) {
            float newWidthRaw =
                MaybeSub(MaybeSub(insetRelativeSize.width, margin.left),
                         margin.right) -
                left.val - right.val;
            knownDimensions.width = Optf(F32Max(newWidthRaw, 0.0f));
            knownDimensions =
                MaybeClamp(knownDimensions.MaybeApplyAspectRatio(aspectRatio),
                           minSize, maxSize);
        }
        if (!knownDimensions.height.IsSome() && top.IsSome() &&
            bottom.IsSome()) {
            float newHeightRaw =
                MaybeSub(MaybeSub(insetRelativeSize.height, margin.top),
                         margin.bottom) -
                top.val - bottom.val;
            knownDimensions.height = Optf(F32Max(newHeightRaw, 0.0f));
            knownDimensions =
                MaybeClamp(knownDimensions.MaybeApplyAspectRatio(aspectRatio),
                           minSize, maxSize);
        }

        SizeAvail childAvail = {
            AvailableSpace::Definite(
                MaybeClamp(containerWidth, minSize.width, maxSize.width)),
            AvailableSpace::Definite(
                MaybeClamp(containerHeight, minSize.height, maxSize.height))};

        SizeF measuredSize = tree->MeasureChildSizeBoth(
            child, knownDimensions, c.nodeInnerSize, childAvail,
            SizingMode::InherentSize, LineBool::False());
        SizeF finalSize = MaybeClamp(knownDimensions.UnwrapOr(measuredSize),
                                     minSize, maxSize);

        LayoutOutput layoutOutput = tree->PerformChildLayout(
            child, AsOptional(finalSize), c.nodeInnerSize, childAvail,
            SizingMode::InherentSize, LineBool::False());

        RectF nonAutoMargin = {
            margin.left.UnwrapOr(0.0f), margin.right.UnwrapOr(0.0f),
            margin.top.UnwrapOr(0.0f), margin.bottom.UnwrapOr(0.0f)};

        SizeF freeSpace = SizeF{c.containerSize.width - finalSize.width -
                                    nonAutoMargin.HorizontalAxisSum(),
                                c.containerSize.height - finalSize.height -
                                    nonAutoMargin.VerticalAxisSum()}
                              .Max(SizeF::Zero());

        // Expand auto margins to fill the available space.
        int autoW =
            (margin.left.IsSome() ? 0 : 1) + (margin.right.IsSome() ? 0 : 1);
        int autoH =
            (margin.top.IsSome() ? 0 : 1) + (margin.bottom.IsSome() ? 0 : 1);
        SizeF autoMarginSize = {
            autoW > 0 ? freeSpace.width / (float)autoW : 0.0f,
            autoH > 0 ? freeSpace.height / (float)autoH : 0.0f};
        RectF resolvedMargin = {margin.left.UnwrapOr(autoMarginSize.width),
                                margin.right.UnwrapOr(autoMarginSize.width),
                                margin.top.UnwrapOr(autoMarginSize.height),
                                margin.bottom.UnwrapOr(autoMarginSize.height)};

        // Flex-relative insets.
        Optf startMain = c.isRow ? left : top;
        Optf endMain = c.isRow ? right : bottom;
        Optf startCross = c.isRow ? top : left;
        Optf endCross = c.isRow ? bottom : right;
        bool mainIsRtl = c.isRow && IsRtl(c.layoutDirection);
        bool crossIsRtl = !c.isRow && IsRtl(c.layoutDirection);
        bool mainAxisFlexStartReversed = IsReverse(c.dir) != mainIsRtl;
        bool crossAxisFlexStartReversed = c.isWrapReverse != crossIsRtl;
        float mainStartScrollbarOffset =
            mainIsRtl ? c.scrollbarGutter.Main(c.dir) : 0.0f;
        float crossStartScrollbarOffset =
            crossIsRtl ? c.scrollbarGutter.Cross(c.dir) : 0.0f;
        float mainEndScrollbarOffset =
            mainIsRtl ? 0.0f : c.scrollbarGutter.Main(c.dir);
        float crossEndScrollbarOffset =
            crossIsRtl ? 0.0f : c.scrollbarGutter.Cross(c.dir);

        float alignedToMainEnd =
            c.containerSize.Main(c.dir) - c.border.MainEnd(c.dir) -
            mainEndScrollbarOffset - finalSize.Main(c.dir) -
            endMain.UnwrapOr(0.0f) - resolvedMargin.MainEnd(c.dir);
        float offsetMain;
        if (startMain.IsSome() || endMain.IsSome()) {
            if (mainIsRtl && endMain.IsSome()) {
                offsetMain = alignedToMainEnd;
            } else if (startMain.IsSome()) {
                offsetMain = startMain.val + c.border.MainStart(c.dir) +
                             mainStartScrollbarOffset +
                             resolvedMargin.MainStart(c.dir);
            } else {
                offsetMain = alignedToMainEnd;
            }
        } else {
            // Stretch is invalid for justify_content in flexbox, so it is
            // treated as unset (FlexStart).
            //
            // The `safe` overflow-position keyword is deliberately NOT applied
            // here even when the item overflows the main axis: Chrome does not
            // apply the safe fallback to justify-content on absolutely
            // positioned flex items (only cross-axis align-self does).
            float startPos = c.contentBoxInset.MainStart(c.dir) +
                             resolvedMargin.MainStart(c.dir);
            float endPos =
                c.containerSize.Main(c.dir) - c.contentBoxInset.MainEnd(c.dir) -
                finalSize.Main(c.dir) - resolvedMargin.MainEnd(c.dir);
            AlignContentKeyword jc =
                c.justifyContent
                    .UnwrapOr(AlignContent{AlignContentKeyword::Start})
                    .Keyword();
            bool rev = mainAxisFlexStartReversed;
            switch (jc) {
                case AlignContentKeyword::SpaceBetween:
                    offsetMain = startPos;
                    break;
                case AlignContentKeyword::Stretch:
                case AlignContentKeyword::FlexStart:
                    offsetMain = rev ? endPos : startPos;
                    break;
                case AlignContentKeyword::FlexEnd:
                    offsetMain = rev ? startPos : endPos;
                    break;
                case AlignContentKeyword::Start:
                    offsetMain = rev ? endPos : startPos;
                    break;
                case AlignContentKeyword::End:
                    offsetMain = rev ? startPos : endPos;
                    break;
                default: // SpaceEvenly, SpaceAround, Center
                    offsetMain = (c.containerSize.Main(c.dir) +
                                  c.contentBoxInset.MainStart(c.dir) -
                                  c.contentBoxInset.MainEnd(c.dir) -
                                  finalSize.Main(c.dir) +
                                  resolvedMargin.MainStart(c.dir) -
                                  resolvedMargin.MainEnd(c.dir)) /
                                 2.0f;
                    break;
            }
        }

        float alignedToCrossEnd =
            c.containerSize.Cross(c.dir) - c.border.CrossEnd(c.dir) -
            crossEndScrollbarOffset - finalSize.Cross(c.dir) -
            endCross.UnwrapOr(0.0f) - resolvedMargin.CrossEnd(c.dir);
        float offsetCross;
        if (startCross.IsSome() || endCross.IsSome()) {
            if (crossIsRtl && endCross.IsSome()) {
                offsetCross = alignedToCrossEnd;
            } else if (startCross.IsSome()) {
                offsetCross = startCross.val + c.border.CrossStart(c.dir) +
                              crossStartScrollbarOffset +
                              resolvedMargin.CrossStart(c.dir);
            } else {
                offsetCross = alignedToCrossEnd;
            }
        } else {
            bool crossOverflows =
                finalSize.Cross(c.dir) + resolvedMargin.CrossAxisSum(c.dir) >
                c.containerSize.Cross(c.dir) - c.contentBoxInset
                                                   .CrossAxisSum(c.dir);
            AlignItemsKeyword ck =
                ResolveSelfAlignmentSafety(alignSelf, crossOverflows);
            float startPos = c.contentBoxInset.CrossStart(c.dir) +
                             resolvedMargin.CrossStart(c.dir);
            float endPos = c.containerSize.Cross(c.dir) -
                           c.contentBoxInset.CrossEnd(c.dir) -
                           finalSize.Cross(c.dir) -
                           resolvedMargin.CrossEnd(c.dir);
            bool rev = crossAxisFlexStartReversed;
            switch (ck) {
                case AlignItemsKeyword::Start:
                    offsetCross = rev ? endPos : startPos;
                    break;
                case AlignItemsKeyword::End:
                    offsetCross = rev ? startPos : endPos;
                    break;
                case AlignItemsKeyword::Center:
                    offsetCross = (c.containerSize.Cross(c.dir) +
                                   c.contentBoxInset.CrossStart(c.dir) -
                                   c.contentBoxInset.CrossEnd(c.dir) -
                                   finalSize.Cross(c.dir) +
                                   resolvedMargin.CrossStart(c.dir) -
                                   resolvedMargin.CrossEnd(c.dir)) /
                                  2.0f;
                    break;
                case AlignItemsKeyword::FlexEnd:
                    offsetCross = rev ? startPos : endPos;
                    break;
                default:
                    // Baseline, Stretch and FlexStart. Stretch alignment does
                    // not apply to absolutely positioned items; see "Example
                    // 3" at https://www.w3.org/TR/css-flexbox-1/#abspos-items
                    offsetCross = rev ? endPos : startPos;
                    break;
            }
        }

        PointF location = c.isRow ? PointF{offsetMain, offsetCross}
                                  : PointF{offsetCross, offsetMain};
        SizeF scrollbarSize = {
            overflow.y == Overflow::Scroll ? scrollbarWidth : 0.0f,
            overflow.x == Overflow::Scroll ? scrollbarWidth : 0.0f};

        Layout layout;
        layout.order = (uint32_t)order;
        layout.size = finalSize;
        layout.contentSize = layoutOutput.contentSize;
        layout.scrollbarSize = scrollbarSize;
        layout.location = location;
        layout.padding = padding;
        layout.border = border;
        layout.margin = resolvedMargin;
        tree->SetUnroundedLayout(child, layout);

        SizeF sizeContribution = {
            overflow.x == Overflow::Visible
                ? F32Max(finalSize.width, layoutOutput.contentSize.width)
                : finalSize.width,
            overflow.y == Overflow::Visible
                ? F32Max(finalSize.height, layoutOutput.contentSize.height)
                : finalSize.height};
        if (sizeContribution.HasNonZeroArea()) {
            PointF absoluteAreaOffset = {
                c.border.left +
                    (IsRtl(c.layoutDirection) ? c.scrollbarGutter.x : 0.0f),
                c.border.top};
            PointF relativeLocation = {location.x - absoluteAreaOffset.x,
                                       location.y - absoluteAreaOffset.y};
            SizeF contribution;
            if (IsRtl(c.layoutDirection)) {
                float overflowExtraWidth =
                    F32Max(sizeContribution.width - finalSize.width, 0.0f);
                contribution.width =
                    F32Max(insetRelativeSize.width - relativeLocation.x, 0.0f) +
                    overflowExtraWidth;
            } else {
                contribution.width = relativeLocation.x + sizeContribution
                                                              .width;
            }
            contribution.height = relativeLocation.y + sizeContribution.height;
            contentSize = contentSize.Max(contribution);
        }
    }

    return contentSize;
}

// ─── compute_preliminary ─────────────────────────────────────────────────

LayoutOutput ComputePreliminary(TaffyTree* tree, NodeId node,
                                const LayoutInput& inputs) {
    SizeOptF knownDimensions = inputs.knownDimensions;
    SizeOptF parentSize = inputs.parentSize;
    RunMode runMode = inputs.runMode;

    AlgoConstants constants = ComputeConstants(tree, tree->GetStyle(node),
                                               knownDimensions, parentSize);

    Vec<FlexItem> flexItems;
    Vec<FlexLine> flexLines;

    // 9.1. Initial Setup
    // 1. Generate anonymous flex items.
    GenerateAnonymousFlexItems(tree, node, constants, &flexItems);

    // 9.2. Line Length Determination
    // 2. Determine the available main and cross space for the flex items.
    SizeAvail availableSpace = DetermineAvailableSpace(
        knownDimensions, inputs.availableSpace, constants);

    // 3. Determine the flex base size and hypothetical main size of each item.
    DetermineFlexBaseSize(tree, constants, availableSpace, flexItems.els,
                          flexItems.len);

    // 4. Determine the main size of the flex container. Already done as part
    //    of ComputeConstants; the inner size is constants.nodeInnerSize.

    // 9.3. Main Size Determination
    // 5. Collect flex items into flex lines.
    CollectFlexLines(constants, availableSpace, &flexItems, &flexLines);

    // If the container size is undefined, determine the container's main size
    // and then re-resolve the gaps against it.
    Optf innerMainKnown = constants.nodeInnerSize.Main(constants.dir);
    if (innerMainKnown.IsSome()) {
        float outerMainSize =
            innerMainKnown.val + constants.contentBoxInset
                                     .MainAxisSum(constants.dir);
        constants.innerContainerSize.SetMain(constants.dir, innerMainKnown.val);
        constants.containerSize.SetMain(constants.dir, outerMainSize);
    } else {
        DetermineContainerMainSize(tree, availableSpace, &flexLines,
                                   &constants);
        constants.nodeInnerSize
            .SetMain(constants.dir,
                     Optf(constants.innerContainerSize.Main(constants.dir)));
        constants.nodeOuterSize.SetMain(
            constants.dir, Optf(constants.containerSize.Main(constants.dir)));

        // Re-resolve percentage gaps against the size just determined.
        const Style& style = tree->GetStyle(node);
        float innerContainerSize = constants.innerContainerSize
                                       .Main(constants.dir);
        SizeF resolvedGap =
            style.gap.ResolveOrZero(Optf(innerContainerSize), tree->calc);
        constants.gap.SetMain(constants.dir, resolvedGap.Main(constants.dir));
    }

    // 6. Resolve the flexible lengths of all the flex items.
    for (int i = 0; i < flexLines.len; i++) {
        ResolveFlexibleLengths(&flexLines[i], constants);
    }

    // 9.4. Cross Size Determination
    // 7. Determine the hypothetical cross size of each item.
    for (int i = 0; i < flexLines.len; i++) {
        DetermineHypotheticalCrossSize(tree, &flexLines[i], constants,
                                       availableSpace);
    }

    // Child baselines, computed only where they are needed.
    CalculateChildrenBaseLines(tree, knownDimensions, availableSpace,
                               &flexLines, constants);

    // 8. Calculate the cross size of each flex line.
    CalculateCrossSize(&flexLines, knownDimensions, constants);

    // 9. Handle align-content: stretch.
    HandleAlignContentStretch(&flexLines, knownDimensions, constants);

    // 10. Collapse visibility:collapse items. Not implemented — taffy does not
    //     support visibility:collapse either.

    // 11. Determine the used cross size of each flex item.
    DetermineUsedCrossSize(tree, &flexLines, constants);

    // 9.5. Main-Axis Alignment
    // 12. Distribute any remaining free space.
    DistributeRemainingFreeSpace(&flexLines, constants);

    // 9.6. Cross-Axis Alignment
    // 13/14. Resolve cross-axis auto margins and align the items.
    ResolveCrossAxisAutoMargins(&flexLines, constants);

    // 15. Determine the flex container's used cross size.
    float totalLineCrossSize =
        DetermineContainerCrossSize(&flexLines, knownDimensions, &constants);

    // The container size is known; a caller that only wanted that is done.
    if (runMode == RunMode::ComputeSize) {
        flexItems.Reset();
        flexLines.Reset();
        return LayoutOutput::FromOuterSize(constants.containerSize);
    }

    // 16. Align all flex lines per align-content.
    AlignFlexLinesPerAlignContent(&flexLines, constants, totalLineCrossSize);

    SizeF inflowContentSize = FinalLayoutPass(tree, &flexLines, constants);

    SizeF absoluteContentSize =
        PerformAbsoluteLayoutOnAbsoluteChildren(tree, node, constants);

    // display:none children still get a zeroed layout.
    int len = tree->ChildCount(node);
    for (int order = 0; order < len; order++) {
        NodeId child = tree->GetChildId(node, order);
        if (tree->GetStyle(child).BoxGenMode() == BoxGenerationMode::None) {
            tree->SetUnroundedLayout(child, Layout::WithOrder((uint32_t)order));
            tree->PerformChildLayout(child, SizeOptF::None(), SizeOptF::None(),
                                     SizeAvail::MaxContent(),
                                     SizingMode::InherentSize,
                                     LineBool::False());
        }
    }

    // 8.5. Flex Container Baselines
    // https://www.w3.org/TR/css-flexbox-1/#flex-baselines
    Optf firstVerticalBaseline;
    if (flexLines.len > 0 && flexLines[0].count > 0) {
        const FlexItem* chosen = nullptr;
        for (int i = 0; i < flexLines[0].count; i++) {
            const FlexItem& item = flexLines[0].items[i];
            if (constants.isColumn ||
                item.alignSelf.keyword == AlignItemsKeyword::Baseline) {
                chosen = &item;
                break;
            }
        }
        if (!chosen) {
            chosen = &flexLines[0].items[0];
        }
        float offsetVertical =
            constants.isRow ? chosen->offsetCross : chosen->offsetMain;
        firstVerticalBaseline = Optf(offsetVertical + chosen->baseline);
    }

    flexItems.Reset();
    flexLines.Reset();

    return LayoutOutput::FromSizesAndBaselines(
        constants.containerSize, inflowContentSize.Max(absoluteContentSize),
        PointOptF{Optf(), firstVerticalBaseline});
}

} // namespace

// ─── compute_flexbox_layout ──────────────────────────────────────────────

LayoutOutput ComputeFlexboxLayout(TaffyTree* tree, NodeId node,
                                  const LayoutInput& inputs) {
    CalcResolver calc = tree->calc;
    SizeOptF knownDimensions = inputs.knownDimensions;
    SizeOptF parentSize = inputs.parentSize;
    RunMode runMode = inputs.runMode;
    const Style& style = tree->GetStyle(node);

    Optf aspectRatio = style.aspectRatio;
    RectF padding = style.padding.ResolveOrZero(parentSize.width, calc);
    RectF border = style.border.ResolveOrZero(parentSize.width, calc);
    SizeF paddingBorderSum = padding.SumAxes() + border.SumAxes();
    SizeF boxSizingAdjustment = style.boxSizing == BoxSizing::ContentBox
                                    ? paddingBorderSum
                                    : SizeF::Zero();

    SizeOptF minSize = MaybeAdd(style.minSize.MaybeResolve(parentSize, calc)
                                    .MaybeApplyAspectRatio(aspectRatio),
                                boxSizingAdjustment);
    SizeOptF maxSize = MaybeAdd(style.maxSize.MaybeResolve(parentSize, calc)
                                    .MaybeApplyAspectRatio(aspectRatio),
                                boxSizingAdjustment);
    SizeOptF clampedStyleSize;
    if (inputs.sizingMode == SizingMode::InherentSize) {
        clampedStyleSize =
            MaybeClamp(MaybeAdd(style.size.MaybeResolve(parentSize, calc)
                                    .MaybeApplyAspectRatio(aspectRatio),
                                boxSizingAdjustment),
                       minSize, maxSize);
    }

    // If both min and max are set in an axis and max <= min, that determines
    // the size in that axis.
    SizeOptF minMaxDefiniteSize;
    if (minSize.width.IsSome() && maxSize.width.IsSome() &&
        maxSize.width.val <= minSize.width.val) {
        minMaxDefiniteSize.width = minSize.width;
    }
    if (minSize.height.IsSome() && maxSize.height.IsSome() &&
        maxSize.height.val <= minSize.height.val) {
        minMaxDefiniteSize.height = minSize.height;
    }

    // The container's size is floored by its padding and border.
    SizeOptF styledBasedKnownDimensions = knownDimensions.Or(
        MaybeMax(minMaxDefiniteSize.Or(clampedStyleSize), paddingBorderSum));

    // Short-circuit when the container's size is fully determined and only
    // the size was asked for.
    if (runMode == RunMode::ComputeSize) {
        if (styledBasedKnownDimensions.BothAxisDefined()) {
            return LayoutOutput::FromOuterSize(
                {styledBasedKnownDimensions.width.val,
                 styledBasedKnownDimensions.height.val});
        }
        if (inputs.axis == RequestedAxis::Horizontal &&
            styledBasedKnownDimensions.width.IsSome()) {
            return LayoutOutput::FromOuterSize(
                {styledBasedKnownDimensions.width.val, 0.0f});
        }
    }

    LayoutInput next = inputs;
    next.knownDimensions = styledBasedKnownDimensions;
    return ComputePreliminary(tree, node, next);
}

} // namespace taffy
