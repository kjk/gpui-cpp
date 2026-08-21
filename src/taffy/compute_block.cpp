/* The CSS block layout algorithm, for a block container holding only
 * block-level boxes — taffy/src/compute/block.rs — together with the float
 * positioning context it uses, taffy/src/compute/float.rs.
 *
 * The two live in one file here because `BlockContext` is the only thing that
 * ever touches a `FloatContext`, and nothing outside block layout names
 * either of them.
 */

#include "taffy/compute.h"

namespace taffy {
namespace {

// ─── float placement — taffy/src/compute/float.rs ────────────────────────
//
// The rules that govern floats are in CSS 2.2 §9.5:
// https://www.w3.org/TR/CSS22/visuren.html#floats

// An empty "slot" that avoids floats, for non-floated content to lay out in.
struct ContentSlot {
    // -1 when the slot is not inside any segment.
    int segmentId = -1;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

struct PlacedFloatedBox {
    float width = 0.0f;
    float height = 0.0f;
    // Distance from the edge the box is floated towards — from the left for
    // left floats, from the right for right floats.
    float xInset = 0.0f;
    // Distance from the top edge of the container.
    float y = 0.0f;
};

// A non-overlapping horizontal band of the block formatting context, with the
// same available width for its whole height.
struct Segment {
    float yStart = 0.0f;
    float yEnd = 0.0f;
    // Left inset in slot 0, right inset in slot 1.
    float insets[2] = {0.0f, 0.0f};

    // Whether the segment can fit the box in the horizontal axis.
    bool FitsFloatWidth(SizeF floatedBox, FloatDirection direction,
                        float bfcWidth) const {
        int slot = (int)direction;
        return insets[slot] == 0.0f ||
               (bfcWidth - floatedBox.w - InsetSum()) >= 0.0f;
    }
    float InsetSum() const { return insets[0] + insets[1]; }
    bool Contains(float y) const { return y >= yStart && y < yEnd; }
};

// Helper for placing one floated box: given a pinned starting y, works out
// whether any x position has room for the box across its whole height.
struct FloatFitter {
    float bfcWidth = 0.0f;
    double slotHeight = 0.0;
    float insets[2] = {0.0f, 0.0f};

    FloatFitter(float bfcWidth_, float slotHeight_, const float in[2])
        : bfcWidth(bfcWidth_), slotHeight((double)slotHeight_) {
        insets[0] = in[0];
        insets[1] = in[1];
    }

    // Union another segment's insets, which is a max on each side.
    void UnionInsets(const float other[2]) {
        insets[0] = F32Max(insets[0], other[0]);
        insets[1] = F32Max(insets[1], other[1]);
    }
    bool FitsHorizontally(float width) const {
        if (insets[0] == 0.0f && insets[1] == 0.0f) {
            return true;
        }
        return bfcWidth - insets[0] - insets[1] - width >= 0.0f;
    }
    void AddHeight(float height) { slotHeight += (double)height; }
    bool FitsVertically(float height) const {
        return slotHeight >= (double)height;
    }
};

struct IndexRange {
    int start = 0;
    int end = 0;
};

// A context for placing floated boxes.
struct FloatContext {
    // The available space constraint of the root block formatting context
    // this manages floats for.
    float availableWidth = 0.0f;
    bool hasFloats = false;
    Vec<PlacedFloatedBox> leftFloats;
    Vec<PlacedFloatedBox> rightFloats;
    // Non-overlapping horizontal bands of the context.
    Vec<Segment> segments;
    // A closed-open range saying which segments the last placed float went
    // into, per side.
    IndexRange lastPlacedFloats[2];

    float LastSegmentEnd() const {
        return segments.len > 0 ? segments[segments.len - 1].yEnd : 0.0f;
    }

    bool HasActiveFloats(float minY) const {
        return hasFloats && LastSegmentEnd() > minY;
    }

    void SetWidth(float w) { availableWidth = w; }

    // Split a segment in two so a new float can start and end on exact
    // segment boundaries.
    void SubdivideSegment(int idx, float divideAtY) {
        Segment newSegment;
        newSegment.insets[0] = segments[idx].insets[0];
        newSegment.insets[1] = segments[idx].insets[1];
        newSegment.yStart = divideAtY;
        newSegment.yEnd = segments[idx].yEnd;
        segments[idx].yEnd = divideAtY;
        segments.InsertAt(idx + 1, newSegment);
    }

    void UpdateLastPlacedFloat(FloatDirection direction, IndexRange placement) {
        int slot = (int)direction;
        IndexRange& r = lastPlacedFloats[slot];
        r.start = r.start > placement.start ? r.start : placement.start;
        r.end = r.end > placement.end ? r.end : placement.end;
    }

    PlacedFloatedBox PlaceFloatedBoxInner(SizeF floatedBox, float minY,
                                          const float containingBlockInsets[2],
                                          FloatDirection direction,
                                          Clear clear);

    PointF PlaceFloatedBox(SizeF floatedBox, float minY,
                           const float containingBlockInsets[2],
                           FloatDirection direction, Clear clear) {
        hasFloats = true;
        PlacedFloatedBox placed = PlaceFloatedBoxInner(
            floatedBox, minY, containingBlockInsets, direction, clear);
        float xInset = placed.xInset;
        float y = placed.y;
        if (direction == FloatDirection::Left) {
            leftFloats.Append(placed);
            return {xInset, y};
        }
        rightFloats.Append(placed);
        return {availableWidth - xInset - floatedBox.w, y};
    }

    // The end segment of the last float on the side(s) `clear` names, or -1.
    int ClearedSegment(Clear clear) const {
        switch (clear) {
            case Clear::Left:
                return lastPlacedFloats[0].end;
            case Clear::Right:
                return lastPlacedFloats[1].end;
            case Clear::Both: {
                int l = lastPlacedFloats[0].end;
                int r = lastPlacedFloats[1].end;
                return l > r ? l : r;
            }
            default:
                return -1;
        }
    }

    // The bottom of the lowest float the clear property is concerned with.
    Optf ClearedThreshold(Clear clear) const {
        int idx = ClearedSegment(clear);
        if (idx < 0) {
            return Optf();
        }
        int at = idx > 1 ? idx - 1 : 0;
        if (at >= segments.len) {
            return Optf();
        }
        return Optf(segments[at].yEnd);
    }

    ContentSlot FindContentSlot(float minY,
                                const float containingBlockInsets[2],
                                Clear clear, int after) const;
};

PlacedFloatedBox FloatContext::PlaceFloatedBoxInner(
    SizeF floatedBox, float minY, const float containingBlockInsets[2],
    FloatDirection direction, Clear clear) {
    int slot = (int)direction;

    // The float must start at or after the last float placed in the same
    // direction, and must respect `clear`.
    int hwm = 0;
    switch (clear) {
        case Clear::Left: {
            int a = lastPlacedFloats[slot].start;
            int b = lastPlacedFloats[0].end + 1;
            hwm = a > b ? a : b;
            break;
        }
        case Clear::Right: {
            int a = lastPlacedFloats[slot].start;
            int b = lastPlacedFloats[1].end + 1;
            hwm = a > b ? a : b;
            break;
        }
        case Clear::Both: {
            int l = lastPlacedFloats[0].end;
            int r = lastPlacedFloats[1].end;
            hwm = (l > r ? l : r) + 1;
            break;
        }
        default:
            hwm = lastPlacedFloats[slot].start;
            break;
    }

    // The float goes in a segment at or below minY.
    int startIdx = segments.len;
    for (int i = hwm; i < segments.len; i++) {
        if (segments[i].yEnd > minY) {
            startIdx = i;
            break;
        }
    }
    float startY = minY;
    int endIdx = startIdx;

    bool haveStart = false;
    bool haveEnd = false;
    int foundStart = 0;
    int foundEnd = 0;
    float placedInset = containingBlockInsets[slot];

    // Find a position with room for the float.
    while (true) {
        // No existing segment can take the float, so it goes into a new one
        // below all of them; that always has room.
        if (startIdx >= segments.len) {
            haveStart = false;
            haveEnd = false;
            placedInset = containingBlockInsets[slot];
            break;
        }

        const Segment& startSegment = segments[startIdx];
        if (!startSegment
                 .FitsFloatWidth(floatedBox, direction, availableWidth)) {
            startIdx++;
            if (endIdx < startIdx) {
                endIdx = startIdx;
            }
            continue;
        }

        startY = F32Max(startY, startSegment.yStart);
        float availableHeight = startSegment.yEnd - startY;
        FloatFitter fitter(availableWidth, availableHeight,
                           containingBlockInsets);
        fitter.UnionInsets(startSegment.insets);

        // With the start segment pinned, walk down to find the end segment:
        // the range must be tall enough for the float and wide enough all the
        // way down.
        bool restartOuter = false;
        while (true) {
            if (endIdx >= segments.len) {
                haveStart = true;
                foundStart = startIdx;
                haveEnd = false;
                placedInset = fitter.insets[slot];
                break;
            }
            const Segment& endSegment = segments[endIdx];
            fitter.UnionInsets(endSegment.insets);
            if (!fitter.FitsHorizontally(floatedBox.w)) {
                startIdx++;
                if (endIdx < startIdx) {
                    endIdx = startIdx;
                }
                restartOuter = true;
                break;
            }
            if (endIdx != startIdx) {
                fitter.AddHeight(endSegment.yEnd - endSegment.yStart);
            }
            if (!fitter.FitsVertically(floatedBox.h)) {
                endIdx++;
                continue;
            }
            haveStart = true;
            foundStart = startIdx;
            haveEnd = true;
            foundEnd = endIdx;
            placedInset = fitter.insets[slot];
            break;
        }
        if (restartOuter) {
            continue;
        }
        break;
    }

    PlacedFloatedBox out;
    out.width = floatedBox.w;
    out.height = floatedBox.h;
    out.y = startY;
    out.xInset = placedInset;

    // A zero-sized box takes up no space and needs no segment.
    if (floatedBox.w == 0.0f || floatedBox.h == 0.0f) {
        return out;
    }

    // Placed after every existing segment.
    if (!haveStart) {
        float lastYEnd = LastSegmentEnd();
        if (startY > lastYEnd) {
            Segment gap;
            gap.yStart = lastYEnd;
            gap.yEnd = startY;
            segments.Append(gap);
        }
        float newStartY = F32Max(lastYEnd, startY);
        Segment seg;
        seg.yStart = newStartY;
        seg.yEnd = newStartY + floatedBox.h;
        seg.insets[0] = containingBlockInsets[0];
        seg.insets[1] = containingBlockInsets[1];
        seg.insets[slot] += floatedBox.w;
        segments.Append(seg);

        int si = segments.len - 1;
        UpdateLastPlacedFloat(direction, {si, si + 1});

        out.y = newStartY;
        out.xInset = containingBlockInsets[slot];
        return out;
    }

    int si = foundStart;
    // If the float does not start exactly on the segment boundary, split the
    // segment there and place it in the second half.
    if (startY != segments[si].yStart) {
        SubdivideSegment(si, startY);
        si++;
        if (haveEnd) {
            foundEnd++;
        }
    }

    int ei;
    if (!haveEnd) {
        float lastYEnd = LastSegmentEnd();
        if (minY > lastYEnd) {
            Segment gap;
            gap.yStart = lastYEnd;
            gap.yEnd = minY;
            segments.Append(gap);
        }
        ei = segments.len - 1;
    } else {
        ei = foundEnd;
        float endY = startY + floatedBox.h;
        if (endY != segments[ei].yEnd) {
            SubdivideSegment(ei, endY);
        }
    }

    float placedInsetPlusWidth = placedInset + floatedBox.w;
    for (int i = si; i <= ei && i < segments.len; i++) {
        segments[i].insets[slot] = placedInsetPlusWidth;
    }

    UpdateLastPlacedFloat(direction, {si, ei + 1});
    return out;
}

ContentSlot FloatContext::FindContentSlot(float minY,
                                          const float containingBlockInsets[2],
                                          Clear clear, int after) const {
    ContentSlot fallback;
    fallback.segmentId = -1;
    fallback.x = containingBlockInsets[0];
    fallback.y = minY;
    fallback.width =
        availableWidth - containingBlockInsets[0] - containingBlockInsets[1];
    fallback.height = INFINITY;

    if (!HasActiveFloats(minY)) {
        return fallback;
    }

    int atLeast = after >= 0 ? after + 1 : 0;
    int cleared = ClearedSegment(clear);
    int hwm = cleared >= 0 ? cleared + 1 : 0;
    if (atLeast > hwm) {
        hwm = atLeast;
    }

    int startIdx = segments.len;
    for (int i = hwm; i < segments.len; i++) {
        if (segments[i].yEnd > minY) {
            startIdx = i;
            break;
        }
    }
    if (startIdx >= segments.len) {
        return fallback;
    }

    const Segment& segment = segments[startIdx];
    float insetLeft = F32Max(segment.insets[0], containingBlockInsets[0]);
    float insetRight = F32Max(segment.insets[1], containingBlockInsets[1]);
    ContentSlot slot;
    slot.segmentId = startIdx;
    slot.x = insetLeft;
    slot.y = F32Max(segment.yStart, minY);
    slot.width = availableWidth - insetLeft - insetRight;
    slot.height = INFINITY;
    return slot;
}

// The intrinsic width contribution of a set of floats.
struct FloatIntrinsicWidthCalculator {
    AvailableSpace availableWidth;
    float contribution = 0.0f;

    void AddFloat(float width) {
        switch (availableWidth.kind) {
            case AvailableSpace::Kind::Definite:
                // Never reached with definite available space.
                break;
            case AvailableSpace::Kind::MinContent:
                contribution = F32Max(contribution, width);
                break;
            default:
                contribution += width;
                break;
        }
    }
    float Result() const { return contribution; }
};

} // namespace

// ─── the block formatting context ────────────────────────────────────────

// Context for positioning block and float boxes within a block formatting
// context. Rust's `BlockFormattingContext`.
struct BlockFormattingContext {
    FloatContext floatContext;
};

// Context for one block within a block formatting context: a pointer to the
// shared BFC plus this block's own offsets. Rust's `BlockContext<'bfc>`.
struct BlockContext {
    BlockFormattingContext* bfc = nullptr;
    // The y offset of this block's border-top, relative to the border-top of
    // the BFC root.
    float yOffset = 0.0f;
    // The x insets of the border box in from each side, relative to the BFC
    // root.
    float insets[2] = {0.0f, 0.0f};
    // The x insets of the content box.
    float contentBoxInsets[2] = {0.0f, 0.0f};
    // The height that floats take up in this element.
    float floatContentContribution = 0.0f;
    bool isRoot = false;

    // A sub-context for a child block node.
    BlockContext SubContext(float additionalYOffset,
                            const float childInsets[2]) {
        BlockContext out;
        out.bfc = bfc;
        out.yOffset = yOffset + additionalYOffset;
        out.insets[0] = insets[0] + childInsets[0];
        out.insets[1] = insets[1] + childInsets[1];
        out.contentBoxInsets[0] = out.insets[0];
        out.contentBoxInsets[1] = out.insets[1];
        out.isRoot = false;
        return out;
    }

    bool IsBfcRoot() const { return isRoot; }

    // The width of the whole BFC, used to resolve positions relative to its
    // right edge (right-floated boxes). Sub-blocks use SubContext instead.
    void SetWidth(float availableWidth) {
        bfc->floatContext.SetWidth(availableWidth);
    }
    // The x-axis content-box insets: the difference between the border box
    // and the content box (padding + border + scrollbar gutter).
    void ApplyContentBoxInset(const float contentBoxXInsets[2]) {
        contentBoxInsets[0] = insets[0] + contentBoxXInsets[0];
        contentBoxInsets[1] = insets[1] + contentBoxXInsets[1];
    }
    bool HasFloats() const { return bfc->floatContext.hasFloats; }
    bool HasActiveFloats(float minY) const {
        return bfc->floatContext.HasActiveFloats(minY + yOffset);
    }
    PointF PlaceFloatedBox(SizeF floatedBox, float minY,
                           FloatDirection direction, Clear clear) {
        PointF pos = bfc->floatContext.PlaceFloatedBox(
            floatedBox, minY + yOffset, contentBoxInsets, direction, clear);
        pos.y -= yOffset;
        pos.x -= insets[0];
        floatContentContribution =
            F32Max(floatContentContribution, pos.y + floatedBox.h);
        return pos;
    }
    ContentSlot FindContentSlot(float minY, Clear clear, int after) const {
        ContentSlot slot = bfc->floatContext.FindContentSlot(
            minY + yOffset, contentBoxInsets, clear, after);
        slot.y -= yOffset;
        slot.x -= insets[0];
        return slot;
    }
    Optf ClearedThreshold(Clear clear) const {
        Optf t = bfc->floatContext.ClearedThreshold(clear);
        if (t.IsSome()) {
            return Optf(t.val - yOffset);
        }
        return Optf();
    }
    void AddChildFloatedContentHeightContribution(float childContribution) {
        floatContentContribution =
            F32Max(floatContentContribution, childContribution);
    }
    float FloatedContentHeightContribution() const {
        return floatContentContribution;
    }
};

namespace {

// ─── block items ─────────────────────────────────────────────────────────

// Per-child data accumulated over the course of the algorithm.
struct BlockItem {
    NodeId nodeId;
    // The index of the item among the children, which controls placement
    // order.
    uint32_t order = 0;

    // Tables do not get stretch sizing applied to them.
    bool isTable = false;
    // Whether the child is a non-independent block or inline node.
    bool isInSameBfc = false;

    Float floatMode = Float::None;
    Clear clear = Clear::None;

    SizeOptF size;
    SizeOptF minSize;
    SizeOptF maxSize;

    PointOverflow overflow;
    float scrollbarWidth = 0.0f;

    Position position = Position::Relative;
    RectLpa inset;
    RectLpa margin;
    RectF padding;
    RectF border;
    SizeF paddingBorderSum;

    SizeF computedSize;
    // The position taking padding, border, margins and scrollbar gutters into
    // account, but not inset.
    PointF staticPosition;
    bool canBeCollapsedThrough = false;

    // Pending layout for in-flow non-floated items, held back from the tree so
    // the post-loop align-content pass can shift location.y first.
    bool hasFinalLayout = false;
    Layout finalLayout;
};

// ─── generate_item_list ──────────────────────────────────────────────────

void GenerateItemList(TaffyTree* tree, NodeId node, SizeOptF nodeInnerSize,
                      Vec<BlockItem>* items) {
    CalcResolver calc = tree->calc;
    int n = tree->ChildCount(node);
    uint32_t order = 0;
    for (int i = 0; i < n; i++) {
        NodeId childNodeId = tree->GetChildId(node, i);
        const Style& cs = tree->GetStyle(childNodeId);
        if (cs.BoxGenMode() == BoxGenerationMode::None) {
            continue;
        }

        Optf aspectRatio = cs.aspectRatio;
        RectF padding = cs.padding.ResolveOrZero(nodeInnerSize, calc);
        RectF border = cs.border.ResolveOrZero(nodeInnerSize, calc);
        SizeF pbSum = (padding + border).SumAxes();
        SizeF boxSizingAdjustment =
            cs.boxSizing == BoxSizing::ContentBox ? pbSum : SizeF::Zero();

        BlockItem item;
        item.nodeId = childNodeId;
        item.order = order++;
        item.isTable = cs.itemIsTable;
        item.floatMode = cs.floatMode;
        item.clear = cs.clear;
        item.position = cs.position;
        item.overflow = cs.overflow;
        item.scrollbarWidth = cs.scrollbarWidth;

        bool isNotFloated = cs.floatMode == Float::None;
        bool isScrollContainer = IsScrollContainer(cs.overflow.x) ||
                                 IsScrollContainer(cs.overflow.y);
        item.isInSameBfc = cs.IsBlock() && !cs.itemIsTable &&
                           cs.position != Position::Absolute && isNotFloated &&
                           !isScrollContainer;

        item.size = MaybeAdd(cs.size.MaybeResolve(nodeInnerSize, calc)
                                 .MaybeApplyAspectRatio(aspectRatio),
                             boxSizingAdjustment);
        item.minSize = MaybeAdd(cs.minSize.MaybeResolve(nodeInnerSize, calc)
                                    .MaybeApplyAspectRatio(aspectRatio),
                                boxSizingAdjustment);
        item.maxSize = MaybeAdd(cs.maxSize.MaybeResolve(nodeInnerSize, calc)
                                    .MaybeApplyAspectRatio(aspectRatio),
                                boxSizingAdjustment);
        item.inset = cs.inset;
        item.margin = cs.margin;
        item.padding = padding;
        item.border = border;
        item.paddingBorderSum = pbSum;
        items->Append(item);
    }
}

// ─── determine_content_based_container_width ─────────────────────────────

float DetermineContentBasedContainerWidth(TaffyTree* tree,
                                          const Vec<BlockItem>& items,
                                          AvailableSpace availableWidth) {
    CalcResolver calc = tree->calc;
    SizeAvail availableSpace = {availableWidth, AvailableSpace::MinContent()};

    float maxChildWidth = 0.0f;
    FloatIntrinsicWidthCalculator floatContribution;
    floatContribution.availableWidth = availableWidth;

    for (int i = 0; i < items.len; i++) {
        const BlockItem& item = items[i];
        if (item.position == Position::Absolute) {
            continue;
        }
        SizeOptF knownDimensions =
            MaybeClamp(item.size, item.minSize, item.maxSize);
        float itemXMarginSum =
            item.margin.ResolveOrZero(availableSpace.width.IntoOption(), calc)
                .HorizontalAxisSum();
        float width;
        if (knownDimensions.width.IsSome()) {
            width = knownDimensions.width.val;
        } else {
            SizeAvail childAvail = availableSpace;
            childAvail.width = MaybeSub(childAvail.width, itemXMarginSum);
            width = tree->MeasureChildSize(
                item.nodeId, knownDimensions, SizeOptF::None(), childAvail,
                SizingMode::InherentSize, AbsoluteAxis::Horizontal,
                LineBool::True());
        }
        width = F32Max(width, item.paddingBorderSum.w) + itemXMarginSum;

        if (IsFloated(item.floatMode)) {
            floatContribution.AddFloat(width);
            continue;
        }
        maxChildWidth = F32Max(maxChildWidth, width);
    }

    return F32Max(maxChildWidth, floatContribution.Result());
}

// ─── perform_final_layout_on_in_flow_children ────────────────────────────

struct InFlowResult {
    SizeF inflowContentSize;
    float intrinsicOuterHeight = 0.0f;
    CollapsibleMarginSet firstChildTopMarginSet;
    CollapsibleMarginSet lastChildBottomMarginSet;
};

InFlowResult PerformFinalLayoutOnInFlowChildren(
    TaffyTree* tree, RunMode runMode, Vec<BlockItem>* items,
    float containerOuterWidth, Optf containerPercentageResolutionHeight,
    RectF contentBoxInset, RectF resolvedContentBoxInset, TextAlign textAlign,
    Direction direction, LineBool ownMarginsCollapseWithChildren,
    BlockContext* blockCtx) {
    CalcResolver calc = tree->calc;
    float containerInnerWidth = containerOuterWidth - resolvedContentBoxInset
                                                          .HorizontalAxisSum();
    Optf percentageResolutionHeight =
        MaybeSub(containerPercentageResolutionHeight, resolvedContentBoxInset
                                                          .VerticalAxisSum());
    SizeOptF parentSize = {Optf(containerInnerWidth),
                           percentageResolutionHeight};
    // Vertical available space in block flow is indefinite, NOT a min-content
    // constraint: MaxContent is taffy's spelling of "indefinite". MinContent
    // here would make every descendant grid believe it was being sized under
    // a min-content constraint, where the maximize-tracks step has no free
    // space, and auto rows holding only scroll containers would collapse.
    SizeAvail availableSpace = {AvailableSpace::Definite(containerInnerWidth),
                                AvailableSpace::MaxContent()};

    // TODO(taffy): handle nested blocks with different widths.
    if (blockCtx->IsBfcRoot()) {
        blockCtx->SetWidth(containerOuterWidth);
        float xInsets[2] = {resolvedContentBoxInset.left,
                            resolvedContentBoxInset.right};
        blockCtx->ApplyContentBoxInset(xInsets);
    }

    InFlowResult res;
    float committedYOffset = resolvedContentBoxInset.top;
    float yOffsetForAbsolute = resolvedContentBoxInset.top;
    CollapsibleMarginSet activeCollapsibleMarginSet;
    bool isCollapsingWithFirstMarginSet = true;
    bool hasActiveFloats = blockCtx->HasActiveFloats(committedYOffset);
    float yOffsetForFloat = resolvedContentBoxInset.top;

    for (int itemIdx = 0; itemIdx < items->len; itemIdx++) {
        BlockItem& item = (*items)[itemIdx];
        if (item.position == Position::Absolute) {
            float x = direction == Direction::Ltr
                          ? resolvedContentBoxInset.left
                          : containerOuterWidth - resolvedContentBoxInset.right;
            item.staticPosition = {x, yOffsetForAbsolute};
            continue;
        }

        RectOptF itemMargin =
            item.margin.MaybeResolve(Optf(containerOuterWidth), calc);
        RectF itemNonAutoMargin = {
            itemMargin.left.UnwrapOr(0.0f), itemMargin.right.UnwrapOr(0.0f),
            itemMargin.top.UnwrapOr(0.0f), itemMargin.bottom.UnwrapOr(0.0f)};
        float itemNonAutoXMarginSum = itemNonAutoMargin.HorizontalAxisSum();

        SizeF scrollbarSize = {
            item.overflow.y == Overflow::Scroll ? item.scrollbarWidth : 0.0f,
            item.overflow.x == Overflow::Scroll ? item.scrollbarWidth : 0.0f};

        // Floated boxes.
        OptFloatDirection floatDirection = FloatDir(item.floatMode);
        if (floatDirection.IsSome()) {
            hasActiveFloats = true;

            LayoutOutput itemLayout = tree->PerformChildLayout(
                item.nodeId, SizeOptF::None(), parentSize,
                SizeAvail::MaxContent(), SizingMode::InherentSize,
                LineBool::True());
            SizeF marginBox = itemLayout.size + itemNonAutoMargin.SumAxes();

            PointF location = blockCtx->PlaceFloatedBox(
                marginBox, yOffsetForFloat, floatDirection.val, item.clear);

            // Turn the margin-box location float placement returned into a
            // border-box location for the output Layout.
            location.y += itemNonAutoMargin.top;
            location.x += itemNonAutoMargin.left;

            Layout layout;
            layout.order = item.order;
            layout.size = itemLayout.size;
            layout.contentSize = itemLayout.contentSize;
            layout.scrollbarSize = scrollbarSize;
            layout.location = location;
            layout.padding = item.padding;
            layout.border = item.border;
            layout.margin = itemNonAutoMargin;
            tree->SetUnroundedLayout(item.nodeId, layout);

            res.inflowContentSize =
                Max(res.inflowContentSize, ComputeContentSizeContribution(
                    location, itemLayout.size, itemLayout.contentSize,
                    item.overflow));
            continue;
        }

        // Non-floated boxes.
        float yMarginOffset = 0.0f;
        float stretchWidth;
        PointF floatAvoidingPosition;
        float floatAvoidingWidth;

        if (item.isInSameBfc) {
            stretchWidth = containerInnerWidth - itemNonAutoXMarginSum;
            floatAvoidingPosition = {0.0f, 0.0f};
            floatAvoidingWidth = 0.0f;
        } else {
            if (!isCollapsingWithFirstMarginSet ||
                !ownMarginsCollapseWithChildren.start) {
                yMarginOffset = activeCollapsibleMarginSet
                                    .CollapseWithMargin(itemNonAutoMargin.top)
                                    .Resolve();
            }
            float minY = committedYOffset + yMarginOffset;
            if (hasActiveFloats) {
                ContentSlot slot = blockCtx
                                       ->FindContentSlot(minY, item.clear, -1);
                hasActiveFloats = slot.segmentId >= 0;
                stretchWidth = slot.width - itemNonAutoXMarginSum;
                floatAvoidingPosition = {slot.x, slot.y};
                floatAvoidingWidth = slot.width;
            } else {
                stretchWidth = containerInnerWidth - itemNonAutoXMarginSum;
                floatAvoidingPosition = {resolvedContentBoxInset.left, minY};
                floatAvoidingWidth = containerInnerWidth;
            }
        }

        SizeOptF knownDimensions;
        if (!item.isTable) {
            // TODO(taffy): allow stretch sizing to be conditional; table
            // children of blocks do not stretch fit.
            SizeOptF sized = item.size;
            sized.width =
                Optf(MaybeClamp(sized.width.UnwrapOr(stretchWidth),
                                item.minSize.width, item.maxSize.width));
            knownDimensions = MaybeClamp(sized, item.minSize, item.maxSize);
        }

        LayoutInput inputs;
        inputs.runMode = runMode;
        inputs.sizingMode = SizingMode::InherentSize;
        inputs.axis = RequestedAxis::Both;
        inputs.knownDimensions = knownDimensions;
        inputs.parentSize = parentSize;
        inputs.availableSpace = availableSpace;
        inputs.availableSpace.width = AvailableSpace::Definite(stretchWidth);
        inputs.verticalMarginsAreCollapsible =
            item.isInSameBfc ? LineBool::True() : LineBool::False();

        float clearPos = blockCtx->ClearedThreshold(item.clear).UnwrapOr(0.0f);

        LayoutOutput itemLayout;
        if (item.isInSameBfc) {
            // A same-BFC child always has a defined width, from stretch
            // sizing.
            float width = knownDimensions.width.UnwrapOr(stretchWidth);

            // TODO(taffy): account for auto margins.
            float insetLeft = itemNonAutoMargin.left + contentBoxInset.left;
            float insetRight = containerOuterWidth - width - insetLeft;
            float insets[2] = {insetLeft, insetRight};

            BlockContext childBlockCtx = blockCtx->SubContext(
                F32Max(yOffsetForAbsolute + itemNonAutoMargin.top, clearPos),
                insets);
            itemLayout = tree->ComputeBlockChildLayout(item.nodeId, inputs,
                                                       &childBlockCtx);
            blockCtx->AddChildFloatedContentHeightContribution(
                yOffsetForAbsolute + childBlockCtx
                                         .FloatedContentHeightContribution());
        } else {
            itemLayout = tree->ComputeChildLayout(item.nodeId, inputs);
        }
        SizeF finalSize = itemLayout.size;

        CollapsibleMarginSet topMarginSet =
            itemLayout.topMargin
                .CollapseWithMargin(itemMargin.top.UnwrapOr(0.0f));
        CollapsibleMarginSet bottomMarginSet =
            itemLayout.bottomMargin
                .CollapseWithMargin(itemMargin.bottom.UnwrapOr(0.0f));

        // Expand auto margins to fill the available space. Vertical auto
        // margins on a relatively positioned block item simply resolve to 0.
        // https://www.w3.org/TR/CSS21/visudet.html#abs-non-replaced-width
        float freeXSpace = F32Max(0.0f, stretchWidth - finalSize.w);
        int autoMarginCount = (itemMargin.left.IsSome() ? 0 : 1) +
                              (itemMargin.right.IsSome() ? 0 : 1);
        float xAxisAutoMarginSize =
            autoMarginCount > 0 ? freeXSpace / (float)autoMarginCount : 0.0f;
        RectF resolvedMargin = {itemMargin.left.UnwrapOr(xAxisAutoMarginSize),
                                itemMargin.right.UnwrapOr(xAxisAutoMarginSize),
                                topMarginSet.Resolve(),
                                bottomMarginSet.Resolve()};

        RectOptF inset = item.inset.MaybeResolveZip(
            {Optf(containerInnerWidth), Optf(0.0f)}, calc);
        Optf negRight = inset.right;
        if (negRight.IsSome()) {
            negRight.val = -negRight.val;
        }
        Optf negBottom = inset.bottom;
        if (negBottom.IsSome()) {
            negBottom.val = -negBottom.val;
        }
        PointF insetOffset = {IsRtl(direction)
                                  ? negRight.Or(inset.left).UnwrapOr(0.0f)
                                  : inset.left.Or(negRight).UnwrapOr(0.0f),
                              inset.top.Or(negBottom).UnwrapOr(0.0f)};

        if (item.isInSameBfc && (!isCollapsingWithFirstMarginSet ||
                                 !ownMarginsCollapseWithChildren.start)) {
            yMarginOffset = activeCollapsibleMarginSet
                                .CollapseWithMargin(resolvedMargin.top)
                                .Resolve();
        }

        bool floatOrNotClear =
            IsFloated(item.floatMode) || item.clear == Clear::None;

        item.computedSize = itemLayout.size;
        item.canBeCollapsedThrough =
            itemLayout.marginsCanCollapseThrough && floatOrNotClear;
        if (item.isInSameBfc) {
            float unclearedY = committedYOffset + activeCollapsibleMarginSet
                                                      .Resolve();
            item.staticPosition = {direction == Direction::Ltr
                                       ? resolvedContentBoxInset.left
                                       : containerOuterWidth -
                                             resolvedContentBoxInset.right -
                                             finalSize.w,
                                   F32Max(unclearedY, clearPos)};
        } else {
            // TODO(taffy): handle inset and margins.
            item.staticPosition = {direction == Direction::Ltr
                                       ? floatAvoidingPosition.x
                                       : floatAvoidingPosition.x +
                                             floatAvoidingWidth -
                                             finalSize.w,
                                   floatAvoidingPosition.y};
        }

        PointF location;
        if (item.isInSameBfc) {
            location = {direction == Direction::Ltr
                            ? resolvedContentBoxInset.left + insetOffset.x +
                                  resolvedMargin.left
                            : containerOuterWidth -
                                  resolvedContentBoxInset.right -
                                  finalSize.w - resolvedMargin.right +
                                  insetOffset.x,
                        F32Max(committedYOffset, clearPos) + yMarginOffset +
                            insetOffset.y};
        } else {
            // TODO(taffy): handle inset and margins.
            location = {direction == Direction::Ltr
                            ? floatAvoidingPosition.x + resolvedMargin.left +
                                  insetOffset.x
                            : floatAvoidingPosition.x + floatAvoidingWidth -
                                  finalSize.w - resolvedMargin.right +
                                  insetOffset.x,
                        floatAvoidingPosition.y + insetOffset.y};
        }

        // Legacy text-align on the block container shifts the item.
        float itemOuterWidth = itemLayout.size.w + resolvedMargin
                                                           .HorizontalAxisSum();
        if (itemOuterWidth < containerInnerWidth) {
            float free = containerInnerWidth - itemOuterWidth;
            switch (textAlign) {
                case TextAlign::LegacyLeft:
                    if (IsRtl(direction)) {
                        location.x -= free;
                    }
                    break;
                case TextAlign::LegacyRight:
                    if (!IsRtl(direction)) {
                        location.x += free;
                    }
                    break;
                case TextAlign::LegacyCenter:
                    location.x += IsRtl(direction) ? -free / 2.0f : free / 2.0f;
                    break;
                default:
                    break;
            }
        }

        // Held back so the align-content pass can shift location.y before the
        // layout is committed to the tree.
        item.hasFinalLayout = true;
        item.finalLayout.order = item.order;
        item.finalLayout.size = itemLayout.size;
        item.finalLayout.contentSize = itemLayout.contentSize;
        item.finalLayout.scrollbarSize = scrollbarSize;
        item.finalLayout.location = location;
        item.finalLayout.padding = item.padding;
        item.finalLayout.border = item.border;
        item.finalLayout.margin = resolvedMargin;

        res.inflowContentSize =
            Max(res.inflowContentSize, ComputeContentSizeContribution(
                {location.x - resolvedContentBoxInset.left,
                 location.y - resolvedContentBoxInset.top},
                finalSize, itemLayout.contentSize, item.overflow));

        if (isCollapsingWithFirstMarginSet) {
            if (item.canBeCollapsedThrough) {
                res.firstChildTopMarginSet =
                    res.firstChildTopMarginSet.CollapseWithSet(topMarginSet)
                        .CollapseWithSet(bottomMarginSet);
            } else {
                res.firstChildTopMarginSet = res.firstChildTopMarginSet
                                                 .CollapseWithSet(topMarginSet);
                isCollapsingWithFirstMarginSet = false;
            }
        }

        if (item.canBeCollapsedThrough) {
            activeCollapsibleMarginSet = activeCollapsibleMarginSet
                                             .CollapseWithSet(topMarginSet)
                                             .CollapseWithSet(bottomMarginSet);
            yOffsetForAbsolute =
                committedYOffset + itemLayout.size.h + yMarginOffset;
            yOffsetForFloat = yOffsetForAbsolute;
        } else {
            committedYOffset =
                location.y - insetOffset.y + itemLayout.size.h;
            activeCollapsibleMarginSet = bottomMarginSet;
            yOffsetForAbsolute = committedYOffset + activeCollapsibleMarginSet
                                                        .Resolve();
            yOffsetForFloat = committedYOffset;
        }
    }

    res.lastChildBottomMarginSet = activeCollapsibleMarginSet;
    float bottomYMarginOffset = ownMarginsCollapseWithChildren.end
                                    ? 0.0f
                                    : res.lastChildBottomMarginSet.Resolve();
    committedYOffset += resolvedContentBoxInset.bottom + bottomYMarginOffset;
    res.intrinsicOuterHeight = F32Max(0.0f, committedYOffset);
    return res;
}

// ─── perform_absolute_layout_on_absolute_children ────────────────────────

SizeF PerformAbsoluteLayoutOnAbsoluteChildren(TaffyTree* tree,
                                              const Vec<BlockItem>& items,
                                              SizeF areaSize, PointF areaOffset,
                                              Direction direction) {
    CalcResolver calc = tree->calc;
    float areaWidth = areaSize.w;
    float areaHeight = areaSize.h;
    SizeF absoluteContentSize = SizeF::Zero();

    for (int i = 0; i < items.len; i++) {
        const BlockItem& item = items[i];
        if (item.position != Position::Absolute) {
            continue;
        }
        const Style& cs = tree->GetStyle(item.nodeId);
        if (cs.BoxGenMode() == BoxGenerationMode::None ||
            cs.position != Position::Absolute) {
            continue;
        }

        Optf aspectRatio = cs.aspectRatio;
        RectOptF margin = cs.margin.MaybeResolve(Optf(areaWidth), calc);
        RectF padding = cs.padding.ResolveOrZero(Optf(areaWidth), calc);
        RectF border = cs.border.ResolveOrZero(Optf(areaWidth), calc);
        SizeF paddingBorderSum = (padding + border).SumAxes();
        SizeF boxSizingAdjustment = cs.boxSizing == BoxSizing::ContentBox
                                        ? paddingBorderSum
                                        : SizeF::Zero();

        RectOptF inset = cs.inset.MaybeResolveZip(AsOptional(areaSize), calc);
        Optf left = inset.left;
        Optf right = inset.right;
        Optf top = inset.top;
        Optf bottom = inset.bottom;

        SizeOptF styleSize =
            MaybeAdd(cs.size.MaybeResolve(AsOptional(areaSize), calc)
                         .MaybeApplyAspectRatio(aspectRatio),
                     boxSizingAdjustment);
        SizeOptF minSize = MaybeMax(
            MaybeAdd(cs.minSize.MaybeResolve(AsOptional(areaSize), calc)
                         .MaybeApplyAspectRatio(aspectRatio),
                     boxSizingAdjustment)
                .Or(AsOptional(paddingBorderSum)),
            paddingBorderSum);
        SizeOptF maxSize =
            MaybeAdd(cs.maxSize.MaybeResolve(AsOptional(areaSize), calc)
                         .MaybeApplyAspectRatio(aspectRatio),
                     boxSizingAdjustment);
        SizeOptF knownDimensions = MaybeClamp(styleSize, minSize, maxSize);

        if (!knownDimensions.width.IsSome() && left.IsSome() &&
            right.IsSome()) {
            float newWidthRaw =
                MaybeSub(MaybeSub(areaWidth, margin.left), margin.right) -
                left.val - right.val;
            knownDimensions.width = Optf(F32Max(newWidthRaw, 0.0f));
            knownDimensions =
                MaybeClamp(knownDimensions.MaybeApplyAspectRatio(aspectRatio),
                           minSize, maxSize);
        }
        if (!knownDimensions.height.IsSome() && top.IsSome() &&
            bottom.IsSome()) {
            float newHeightRaw =
                MaybeSub(MaybeSub(areaHeight, margin.top), margin.bottom) -
                top.val - bottom.val;
            knownDimensions.height = Optf(F32Max(newHeightRaw, 0.0f));
            knownDimensions =
                MaybeClamp(knownDimensions.MaybeApplyAspectRatio(aspectRatio),
                           minSize, maxSize);
        }

        SizeAvail childAvail = {
            AvailableSpace::Definite(
                MaybeClamp(areaWidth, minSize.width, maxSize.width)),
            AvailableSpace::Definite(
                MaybeClamp(areaHeight, minSize.height, maxSize.height))};

        SizeF measuredSize = tree->MeasureChildSizeBoth(
            item.nodeId, knownDimensions, AsOptional(areaSize), childAvail,
            SizingMode::ContentSize, LineBool::False());
        SizeF finalSize = MaybeClamp(knownDimensions.UnwrapOr(measuredSize),
                                     minSize, maxSize);

        LayoutOutput layoutOutput = tree->PerformChildLayout(
            item.nodeId, AsOptional(finalSize), AsOptional(areaSize),
            childAvail, SizingMode::ContentSize, LineBool::False());

        RectF nonAutoMargin = {
            left.IsSome() ? margin.left.UnwrapOr(0.0f) : 0.0f,
            right.IsSome() ? margin.right.UnwrapOr(0.0f) : 0.0f,
            top.IsSome() ? margin.top.UnwrapOr(0.0f) : 0.0f,
            bottom.IsSome() ? margin.bottom.UnwrapOr(0.0f) : 0.0f};

        // Auto margins on an absolutely positioned element in a block
        // container only resolve if the matching inset is set; otherwise they
        // resolve to 0.
        // https://www.w3.org/TR/CSS21/visudet.html#abs-non-replaced-width
        PointF absoluteAutoMarginSpace = {
            right.IsSome() ? areaSize.w - right.val - left.UnwrapOr(0.0f)
                           : finalSize.w,
            bottom.IsSome() ? areaSize.h - bottom.val - top.UnwrapOr(0.0f)
                            : finalSize.h};
        SizeF freeSpace = {absoluteAutoMarginSpace.x - finalSize.w -
                               nonAutoMargin.HorizontalAxisSum(),
                           absoluteAutoMarginSpace.y - finalSize.h -
                               nonAutoMargin.VerticalAxisSum()};

        int autoW =
            (margin.left.IsSome() ? 0 : 1) + (margin.right.IsSome() ? 0 : 1);
        int autoH =
            (margin.top.IsSome() ? 0 : 1) + (margin.bottom.IsSome() ? 0 : 1);
        SizeF autoMarginSize;
        if (autoW == 2 && (!styleSize.width.IsSome() ||
                           styleSize.width.val >= freeSpace.w)) {
            autoMarginSize.w = 0.0f;
        } else if (autoW > 0) {
            autoMarginSize.w = freeSpace.w / (float)autoW;
        }
        if (autoH == 2 && (!styleSize.height.IsSome() ||
                           styleSize.height.val >= freeSpace.h)) {
            autoMarginSize.h = 0.0f;
        } else if (autoH > 0) {
            autoMarginSize.h = freeSpace.h / (float)autoH;
        }
        RectF autoMargin = {
            margin.left.IsSome() ? 0.0f : autoMarginSize.w,
            margin.right.IsSome() ? 0.0f : autoMarginSize.w,
            margin.top.IsSome() ? 0.0f : autoMarginSize.h,
            margin.bottom.IsSome() ? 0.0f : autoMarginSize.h};
        RectF resolvedMargin = {margin.left.UnwrapOr(autoMargin.left),
                                margin.right.UnwrapOr(autoMargin.right),
                                margin.top.UnwrapOr(autoMargin.top),
                                margin.bottom.UnwrapOr(autoMargin.bottom)};

        float xOffset;
        if (left.IsSome() && right.IsSome()) {
            xOffset = IsRtl(direction) ? areaSize.w - finalSize.w -
                                             right.val - resolvedMargin.right
                                       : left.val + resolvedMargin.left;
        } else if (left.IsSome()) {
            xOffset = left.val + resolvedMargin.left;
        } else if (right.IsSome()) {
            xOffset = areaSize.w - finalSize.w - right.val -
                      resolvedMargin.right;
        } else {
            xOffset = IsRtl(direction)
                          ? item.staticPosition.x - finalSize.w -
                                resolvedMargin.right - areaOffset.x
                          : item.staticPosition.x + resolvedMargin.left -
                                areaOffset.x;
        }

        float yLocation;
        if (top.IsSome()) {
            yLocation = top.val + resolvedMargin.top + areaOffset.y;
        } else if (bottom.IsSome()) {
            yLocation = areaSize.h - finalSize.h - bottom.val -
                        resolvedMargin.bottom + areaOffset.y;
        } else {
            yLocation = item.staticPosition.y + resolvedMargin.top;
        }
        PointF location = {xOffset + areaOffset.x, yLocation};

        // The axes are switched: a scrollbar takes space in the axis opposite
        // the one it scrolls.
        SizeF scrollbarSize = {
            item.overflow.y == Overflow::Scroll ? item.scrollbarWidth : 0.0f,
            item.overflow.x == Overflow::Scroll ? item.scrollbarWidth : 0.0f};

        Layout layout;
        layout.order = item.order;
        layout.size = finalSize;
        layout.contentSize = layoutOutput.contentSize;
        layout.scrollbarSize = scrollbarSize;
        layout.location = location;
        layout.padding = padding;
        layout.border = border;
        layout.margin = resolvedMargin;
        tree->SetUnroundedLayout(item.nodeId, layout);

        PointF relativeLocation = {location.x - areaOffset.x,
                                   location.y - areaOffset.y};
        absoluteContentSize =
            Max(absoluteContentSize,
                ComputeContentSizeContribution(relativeLocation, finalSize,
                                               layoutOutput.contentSize,
                                               item.overflow));
    }

    return absoluteContentSize;
}

// ─── compute_inner ───────────────────────────────────────────────────────

LayoutOutput ComputeInner(TaffyTree* tree, NodeId nodeId,
                          const LayoutInput& inputs, BlockContext* blockCtx) {
    CalcResolver calc = tree->calc;
    SizeOptF knownDimensionsIn = inputs.knownDimensions;
    SizeOptF parentSize = inputs.parentSize;
    SizeAvail availableSpace = inputs.availableSpace;
    RunMode runMode = inputs.runMode;
    LineBool verticalMarginsAreCollapsible = inputs
                                                 .verticalMarginsAreCollapsible;

    const Style& style = tree->GetStyle(nodeId);
    RectLp rawPadding = style.padding;
    RectLp rawBorder = style.border;
    RectLpa rawMargin = style.margin;
    Optf aspectRatio = style.aspectRatio;
    RectF padding = rawPadding.ResolveOrZero(parentSize.width, calc);
    RectF border = rawBorder.ResolveOrZero(parentSize.width, calc);
    Direction direction = style.direction;

    // A node that scrolls vertically needs *horizontal* space reserved for
    // its scrollbar, hence the transposed axes.
    PointOverflow t = style.overflow.Transpose();
    PointF offsets = {t.x == Overflow::Scroll ? style.scrollbarWidth : 0.0f,
                      t.y == Overflow::Scroll ? style.scrollbarWidth : 0.0f};
    RectF scrollbarGutter = direction == Direction::Ltr
                                ? RectF{0.0f, offsets.x, 0.0f, offsets.y}
                                : RectF{offsets.x, 0.0f, 0.0f, offsets.y};
    RectF paddingBorder = padding + border;
    SizeF paddingBorderSize = paddingBorder.SumAxes();
    RectF contentBoxInset = paddingBorder + scrollbarGutter;

    float xInsets[2] = {contentBoxInset.left, contentBoxInset.right};
    blockCtx->ApplyContentBoxInset(xInsets);

    SizeF boxSizingAdjustment = style.boxSizing == BoxSizing::ContentBox
                                    ? paddingBorderSize
                                    : SizeF::Zero();
    SizeOptF size = MaybeAdd(style.size.MaybeResolve(parentSize, calc)
                                 .MaybeApplyAspectRatio(aspectRatio),
                             boxSizingAdjustment);
    SizeOptF minSize = MaybeAdd(style.minSize.MaybeResolve(parentSize, calc)
                                    .MaybeApplyAspectRatio(aspectRatio),
                                boxSizingAdjustment);
    SizeOptF maxSize = MaybeAdd(style.maxSize.MaybeResolve(parentSize, calc)
                                    .MaybeApplyAspectRatio(aspectRatio),
                                boxSizingAdjustment);

    // css-sizing-4: a definite size in one axis transfers through
    // `aspect-ratio` to make the other definite. Deriving it from
    // knownDimensions self-gates the transfer — a block parent fills an axis
    // only when that is a real constraint (the stretched width at final
    // layout) and leaves it None while probing intrinsic sizes, so measure
    // passes stay content-based. Only a newly-filled axis is adopted and
    // clamped; an incoming known size is left as the parent resolved it,
    // since re-clamping would undo padding/border overrides.
    SizeOptF derived = MaybeClamp(
        knownDimensionsIn.MaybeApplyAspectRatio(aspectRatio), minSize, maxSize);
    SizeOptF knownDimensions = {knownDimensionsIn.width.Or(derived.width),
                                knownDimensionsIn.height.Or(derived.height)};
    SizeOptF containerContentBoxSize =
        MaybeSub(knownDimensions, contentBoxInset.SumAxes());

    bool isScrollContainer = IsScrollContainer(style.overflow.x) ||
                             IsScrollContainer(style.overflow.y);

    LineBool ownMarginsCollapseWithChildren = {
        verticalMarginsAreCollapsible.start && !isScrollContainer &&
            style.position == Position::Relative && padding.top == 0.0f &&
            border.top == 0.0f,
        verticalMarginsAreCollapsible.end && !isScrollContainer &&
            style.position == Position::Relative && padding.bottom == 0.0f &&
            border.bottom == 0.0f && !size.height.IsSome()};
    bool hasStylesPreventingBeingCollapsedThrough =
        !style.IsBlock() || blockCtx->IsBfcRoot() || isScrollContainer ||
        style.position == Position::Absolute || padding.top > 0.0f ||
        padding.bottom > 0.0f || border.top > 0.0f || border.bottom > 0.0f ||
        (size.height.IsSome() && size.height.val > 0.0f) ||
        (minSize.height.IsSome() && minSize.height.val > 0.0f);

    TextAlign textAlign = style.textAlign;
    OptAlignContent alignContent = style.alignContent;

    // 1. Generate items.
    Vec<BlockItem> items;
    GenerateItemList(tree, nodeId, containerContentBoxSize, &items);

    // 2. Compute the container width.
    float containerOuterWidth;
    if (knownDimensions.width.IsSome()) {
        containerOuterWidth = knownDimensions.width.val;
    } else {
        AvailableSpace availableWidth =
            MaybeSub(availableSpace.width, contentBoxInset.HorizontalAxisSum());
        float intrinsicWidth =
            DetermineContentBasedContainerWidth(tree, items, availableWidth) +
            contentBoxInset.HorizontalAxisSum();
        containerOuterWidth =
            MaybeMax(MaybeClamp(intrinsicWidth, minSize.width, maxSize.width),
                     Optf(paddingBorderSize.w));
    }

    if (runMode == RunMode::ComputeSize && knownDimensions.height.IsSome()) {
        return LayoutOutput::FromOuterSize(
            {containerOuterWidth, knownDimensions.height.val});
    }
    if (runMode == RunMode::ComputeSize &&
        inputs.axis == RequestedAxis::Horizontal) {
        return LayoutOutput::FromOuterSize({containerOuterWidth, 0.0f});
    }

    Optf containerPercentageResolutionHeight =
        knownDimensions.height.Or(MaybeMax(size.height, minSize.height))
            .Or(minSize.height);

    // 3. Final item layout.
    RectF resolvedPadding = rawPadding
                                .ResolveOrZero(Optf(containerOuterWidth), calc);
    RectF resolvedBorder = rawBorder
                               .ResolveOrZero(Optf(containerOuterWidth), calc);
    RectF resolvedContentBoxInset =
        resolvedPadding + resolvedBorder + scrollbarGutter;

    InFlowResult inFlow = PerformFinalLayoutOnInFlowChildren(
        tree, runMode, &items, containerOuterWidth,
        containerPercentageResolutionHeight, contentBoxInset,
        resolvedContentBoxInset, textAlign, direction,
        ownMarginsCollapseWithChildren, blockCtx);
    SizeF inflowContentSize = inFlow.inflowContentSize;
    float intrinsicOuterHeight = inFlow.intrinsicOuterHeight;

    // A root BFC contains its floats.
    if (blockCtx->IsBfcRoot() || isScrollContainer) {
        intrinsicOuterHeight = F32Max(
            intrinsicOuterHeight, blockCtx->FloatedContentHeightContribution());
    }

    float containerOuterHeight =
        MaybeMax(knownDimensions.height.UnwrapOr(MaybeClamp(
                     intrinsicOuterHeight, minSize.height, maxSize.height)),
                 Optf(paddingBorderSize.h));
    SizeF finalOuterSize = {containerOuterWidth, containerOuterHeight};

    // Apply align-content to the in-flow non-floated items. Block layout
    // treats the whole stack of in-flow children as one alignment subject, so
    // the distribution keywords must take the single-subject fallback
    // unconditionally — which is what passing numItems = 1 does. The group
    // then shifts by one offset, with no inter-item gap.
    if (alignContent.IsSome()) {
        float containerInnerHeight =
            containerOuterHeight - resolvedContentBoxInset.VerticalAxisSum();
        float inflowContentHeight =
            intrinsicOuterHeight - resolvedContentBoxInset.VerticalAxisSum();
        float freeSpace = containerInnerHeight - inflowContentHeight;
        bool anyInFlow = false;
        for (int i = 0; i < items.len; i++) {
            if (items[i].hasFinalLayout) {
                anyInFlow = true;
                break;
            }
        }
        if (anyInFlow) {
            AlignContentKeyword keyword =
                ApplyAlignmentFallback(freeSpace, 1, alignContent.val);
            float groupOffset = ComputeAlignmentOffset(freeSpace, 1, 0.0f,
                                                       keyword, false, true);
            for (int i = 0; i < items.len; i++) {
                if (items[i].hasFinalLayout) {
                    items[i].finalLayout.location.y += groupOffset;
                }
            }
            inflowContentSize = SizeF::Zero();
            for (int i = 0; i < items.len; i++) {
                if (!items[i].hasFinalLayout) {
                    continue;
                }
                const Layout& l = items[i].finalLayout;
                inflowContentSize =
                    Max(inflowContentSize, ComputeContentSizeContribution(
                        {l.location.x - resolvedContentBoxInset.left,
                         l.location.y - resolvedContentBoxInset.top},
                        l.size, l.contentSize, items[i].overflow));
            }
        }
    }

    bool allInFlowChildrenCanBeCollapsedThrough = true;
    for (int i = 0; i < items.len; i++) {
        if (items[i].position != Position::Absolute &&
            !items[i].canBeCollapsedThrough) {
            allInFlowChildrenCanBeCollapsedThrough = false;
            break;
        }
    }
    bool canBeCollapsedThrough = !hasStylesPreventingBeingCollapsedThrough &&
                                 allInFlowChildrenCanBeCollapsedThrough;

    LayoutOutput output;
    output.size = finalOuterSize;
    output.topMargin = ownMarginsCollapseWithChildren.start
                           ? inFlow.firstChildTopMarginSet
                           : CollapsibleMarginSet::FromMargin(
                                 rawMargin.MaybeResolve(parentSize.width, calc)
                                     .top.UnwrapOr(0.0f));
    output
        .bottomMargin = ownMarginsCollapseWithChildren.end
                            ? inFlow.lastChildBottomMarginSet
                            : CollapsibleMarginSet::FromMargin(
                                  rawMargin.MaybeResolve(parentSize.width, calc)
                                      .bottom.UnwrapOr(0.0f));
    output.marginsCanCollapseThrough = canBeCollapsedThrough;

    // The margin-collapsing outputs matter to the parent's intrinsic height,
    // so they are returned even when only the size was asked for.
    if (runMode == RunMode::ComputeSize) {
        return output;
    }

    // Commit the deferred in-flow layouts. Floated items already wrote theirs.
    for (int i = 0; i < items.len; i++) {
        if (items[i].hasFinalLayout) {
            tree->SetUnroundedLayout(items[i].nodeId, items[i].finalLayout);
        }
    }

    // 4. Absolutely positioned children.
    RectF absolutePositionInset = resolvedBorder + scrollbarGutter;
    SizeF absolutePositionArea = finalOuterSize - absolutePositionInset
                                                      .SumAxes();
    PointF absolutePositionOffset = {absolutePositionInset.left,
                                     absolutePositionInset.top};
    SizeF absoluteContentSize = PerformAbsoluteLayoutOnAbsoluteChildren(
        tree, items, absolutePositionArea, absolutePositionOffset, direction);

    output.contentSize = Max(inflowContentSize, absoluteContentSize);

    // 5. Hidden children.
    int len = tree->ChildCount(nodeId);
    for (int order = 0; order < len; order++) {
        NodeId child = tree->GetChildId(nodeId, order);
        if (tree->GetStyle(child).BoxGenMode() == BoxGenerationMode::None) {
            tree->SetUnroundedLayout(child, Layout::WithOrder((uint32_t)order));
            tree->PerformChildLayout(child, SizeOptF::None(), SizeOptF::None(),
                                     SizeAvail::MaxContent(),
                                     SizingMode::InherentSize,
                                     LineBool::False());
        }
    }

    return output;
}

} // namespace

// ─── compute_block_layout ────────────────────────────────────────────────

LayoutOutput ComputeBlockLayout(TaffyTree* tree, NodeId nodeId,
                                const LayoutInput& inputs,
                                BlockContext* blockCtx) {
    CalcResolver calc = tree->calc;
    SizeOptF knownDimensions = inputs.knownDimensions;
    SizeOptF parentSize = inputs.parentSize;
    RunMode runMode = inputs.runMode;
    const Style& style = tree->GetStyle(nodeId);

    bool isScrollContainer = IsScrollContainer(style.overflow.x) ||
                             IsScrollContainer(style.overflow.y);
    Optf aspectRatio = style.aspectRatio;
    RectF padding = style.padding.ResolveOrZero(parentSize.width, calc);
    RectF border = style.border.ResolveOrZero(parentSize.width, calc);
    SizeF paddingBorderSize = (padding + border).SumAxes();
    SizeF boxSizingAdjustment = style.boxSizing == BoxSizing::ContentBox
                                    ? paddingBorderSize
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

    SizeOptF styledBasedKnownDimensions =
        MaybeMax(knownDimensions.Or(minMaxDefiniteSize).Or(clampedStyleSize),
                 paddingBorderSize);

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

    // Use the block formatting context that was passed down, unless this node
    // is a scroll container, in which case it starts one of its own.
    if (blockCtx && !isScrollContainer) {
        return ComputeInner(tree, nodeId, next, blockCtx);
    }
    BlockFormattingContext rootBfc;
    BlockContext rootCtx;
    rootCtx.bfc = &rootBfc;
    rootCtx.isRoot = true;
    LayoutOutput out = ComputeInner(tree, nodeId, next, &rootCtx);
    rootBfc.floatContext.leftFloats.Reset();
    rootBfc.floatContext.rightFloats.Reset();
    rootBfc.floatContext.segments.Reset();
    return out;
}

} // namespace taffy
