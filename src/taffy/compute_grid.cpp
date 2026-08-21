/* The CSS Grid layout algorithm — taffy/src/compute/grid/*, a partial
 * implementation of CSS Grid Level 1: https://www.w3.org/TR/css-grid-1
 *
 * Rust splits this over `mod.rs`, `alignment.rs`, `explicit_grid.rs`,
 * `implicit_grid.rs`, `placement.rs`, `track_sizing.rs` and `types/*.rs`. They
 * are one file here, in that order, because every one of them is private to
 * the algorithm and none of them is named from outside it.
 *
 * Deviations from the Rust:
 *   - `detailed_layout_info` is not produced. It exists for consumers that
 *     want the computed track sizes back out; nothing in this tree reads it.
 *   - The `NamedLineResolver` keys its maps on `Str` plus a `-start`/`-end`
 *     marker rather than building the joined string, so resolving a named line
 *     allocates nothing.
 *   - Rust's closures for "which tracks are affected" and "what is a track's
 *     limit" are C++ lambdas passed to function templates, which compile away
 *     the same way.
 */

#include "taffy/compute.h"

namespace taffy {
namespace {

// ─── types/grid_track_counts.rs ──────────────────────────────────────────
//
// Taffy uses two coordinate systems for grid *tracks*, both covering the whole
// implicit grid:
//
//   CellOccupancyMatrix indices: 0 is the leftmost track of the implicit grid.
//   GridTrackVec indices: the vector stores lines and tracks together, so even
//   indices are lines and odd indices are tracks. Index 1 is the leftmost
//   track, index 0 the leftmost line.

// The number of tracks in a dimension, split by implicit and explicit.
struct TrackCounts {
    uint16_t negativeImplicit = 0;
    uint16_t explicitCount = 0;
    uint16_t positiveImplicit = 0;

    static TrackCounts FromRaw(uint16_t neg, uint16_t exp, uint16_t pos) {
        return {neg, exp, pos};
    }
    int Len() const {
        return (int)negativeImplicit + (int)explicitCount +
               (int)positiveImplicit;
    }
    OriginZeroLine ImplicitStartLine() const {
        return OriginZeroLine{(int16_t)(-(int16_t)negativeImplicit)};
    }
    OriginZeroLine ImplicitEndLine() const {
        return OriginZeroLine{(int16_t)(explicitCount + positiveImplicit)};
    }
    // The track immediately after a line, as a matrix index.
    int16_t OzLineToNextTrack(OriginZeroLine l) const {
        return (int16_t)(l.v + (int16_t)negativeImplicit);
    }
    // The line immediately before a track, in OriginZero coordinates.
    OriginZeroLine TrackToPrevOzLine(uint16_t index) const {
        return OriginZeroLine{
            (int16_t)((int16_t)index - (int16_t)negativeImplicit)};
    }
};

// The GridTrackVec index of an OriginZero line. Rust panics when the line is
// out of range; the fallible form below is what absolutely positioned items
// use.
int IntoTrackVecIndex(OriginZeroLine l, TrackCounts counts) {
    return 2 * (int)(l.v + (int16_t)counts.negativeImplicit);
}

bool TryIntoTrackVecIndex(OriginZeroLine l, TrackCounts counts, int* out) {
    if (l.v < -(int16_t)counts.negativeImplicit) {
        return false;
    }
    if (l.v > (int16_t)(counts.explicitCount + counts.positiveImplicit)) {
        return false;
    }
    *out = 2 * (int)(l.v + (int16_t)counts.negativeImplicit);
    return true;
}

// ─── types/grid_track.rs ─────────────────────────────────────────────────

// Rust's `Line<u16>`: an item's placement as GridTrackVec indices.
struct LineU16 {
    uint16_t start = 0;
    uint16_t end = 0;
};

enum class GridTrackKind : uint8_t {
    Track,
    Gutter
};

// One grid track (row/column). Gutters are sized like tracks, so they are the
// same struct.
struct GridTrack {
    GridTrackKind kind = GridTrackKind::Track;
    // A collapsed track is treated as if it did not exist for sizing.
    bool isCollapsed = false;
    MinTrackSizingFunction minTrackSizingFunction;
    MaxTrackSizingFunction maxTrackSizingFunction;
    // The distance from the start of the grid container.
    float offset = 0.0f;
    float baseSize = 0.0f;
    // Scratch; can be infinity.
    float growthLimit = 0.0f;
    // Scratch: an extra amount added to the estimate of the available space in
    // the opposite axis when content-sizing items.
    float contentAlignmentAdjustment = 0.0f;
    // Scratch, while distributing space.
    float itemIncurredIncrease = 0.0f;
    float baseSizePlannedIncrease = 0.0f;
    float growthLimitPlannedIncrease = 0.0f;
    // https://www.w3.org/TR/css3-grid-layout/#infinitely-growable
    bool infinitelyGrowable = false;

    static GridTrack New(MinTrackSizingFunction mn, MaxTrackSizingFunction mx) {
        GridTrack t;
        t.kind = GridTrackKind::Track;
        t.minTrackSizingFunction = mn;
        t.maxTrackSizingFunction = mx;
        return t;
    }
    static GridTrack Gutter(LengthPercentage size) {
        GridTrack t;
        t.kind = GridTrackKind::Gutter;
        t.minTrackSizingFunction = MinTrackSizingFunction::From(size);
        t.maxTrackSizingFunction = MaxTrackSizingFunction::From(size);
        return t;
    }
    void Collapse() {
        isCollapsed = true;
        minTrackSizingFunction = MinTrackSizingFunction::Zero();
        maxTrackSizingFunction = MaxTrackSizingFunction::Zero();
    }
    bool IsFlexible() const { return maxTrackSizingFunction.IsFr(); }
    bool UsesPercentage() const {
        return minTrackSizingFunction.UsesPercentage() || maxTrackSizingFunction
                                                              .UsesPercentage();
    }
    bool HasIntrinsicSizingFunction() const {
        return minTrackSizingFunction.raw.IsIntrinsic() ||
               maxTrackSizingFunction.IsIntrinsic();
    }
    float FitContentLimit(Optf axisAvailableGridSpace) const {
        switch (maxTrackSizingFunction.raw.Tag()) {
            case CompactLength::kFitContentPxTag:
                return maxTrackSizingFunction.raw.Value();
            case CompactLength::kFitContentPercentTag:
                return axisAvailableGridSpace.IsSome()
                           ? axisAvailableGridSpace.val * maxTrackSizingFunction
                                                              .raw.Value()
                           : INFINITY;
            default:
                return INFINITY;
        }
    }
    float FitContentLimitedGrowthLimit(Optf axisAvailableGridSpace) const {
        return F32Min(growthLimit, FitContentLimit(axisAvailableGridSpace));
    }
    float FlexFactor() const {
        return maxTrackSizingFunction.IsFr()
                   ? maxTrackSizingFunction.raw.Value()
                   : 0.0f;
    }
};

// ─── types/cell_occupancy.rs ─────────────────────────────────────────────

enum class CellOccupancyState : uint8_t {
    Unoccupied,
    DefinitelyPlaced,
    AutoPlaced
};

// A dynamically sized matrix tracking the occupancy of each cell during
// auto-placement, plus how many tracks are implicit and explicit.
struct CellOccupancyMatrix {
    Vec<uint8_t> inner;
    int nRows = 0;
    int nCols = 0;
    TrackCounts columns;
    TrackCounts rows;

    void Init(TrackCounts cols, TrackCounts rws) {
        columns = cols;
        rows = rws;
        nRows = rws.Len();
        nCols = cols.Len();
        inner.Reset();
        int n = nRows * nCols;
        if (n > 0) {
            uint8_t* p = inner.AppendBlanks(n);
            if (p) {
                memset(p, 0, (size_t)n);
            }
        }
    }
    void Free() { inner.Reset(); }

    CellOccupancyState Get(int row, int col) const {
        if (row < 0 || row >= nRows || col < 0 || col >= nCols) {
            return CellOccupancyState::Unoccupied;
        }
        return (CellOccupancyState)inner[row * nCols + col];
    }
    void Set(int row, int col, CellOccupancyState v) {
        if (row < 0 || row >= nRows || col < 0 || col >= nCols) {
            return;
        }
        inner[row * nCols + col] = (uint8_t)v;
    }
    const TrackCounts& Counts(AbsoluteAxis a) const {
        return a == AbsoluteAxis::Horizontal ? columns : rows;
    }

    bool IsAreaInRange(AbsoluteAxis primaryAxis, int primaryStart,
                       int primaryEnd, int secondaryStart,
                       int secondaryEnd) const {
        if (primaryStart < 0 || primaryEnd > Counts(primaryAxis).Len()) {
            return false;
        }
        if (secondaryStart < 0 || secondaryEnd > Counts(OtherAxis(primaryAxis))
                                                     .Len()) {
            return false;
        }
        return true;
    }

    // Grow the matrix (in any of the four directions) so the given ranges fit.
    void ExpandToFitRange(int rowStart, int rowEnd, int colStart, int colEnd) {
        int reqNegRows = rowStart < 0 ? -rowStart : 0;
        int reqPosRows = rowEnd - rows.Len() > 0 ? rowEnd - rows.Len() : 0;
        int reqNegCols = colStart < 0 ? -colStart : 0;
        int reqPosCols =
            colEnd - columns.Len() > 0 ? colEnd - columns.Len() : 0;

        int oldRowCount = nRows;
        int oldColCount = nCols;
        int newRowCount = oldRowCount + reqNegRows + reqPosRows;
        int newColCount = oldColCount + reqNegCols + reqPosCols;

        Vec<uint8_t> data;
        uint8_t* p = data.AppendBlanks(newRowCount * newColCount);
        if (!p) {
            return;
        }
        memset(p, 0, (size_t)(newRowCount * newColCount));
        for (int row = 0; row < oldRowCount; row++) {
            for (int col = 0; col < oldColCount; col++) {
                p[(row + reqNegRows) * newColCount + (col + reqNegCols)] =
                    inner[row * nCols + col];
            }
        }
        inner.Reset();
        inner = data;
        data.els = nullptr;
        data.len = 0;
        data.cap = 0;

        nRows = newRowCount;
        nCols = newColCount;
        rows.negativeImplicit = (uint16_t)(rows.negativeImplicit + reqNegRows);
        rows.positiveImplicit = (uint16_t)(rows.positiveImplicit + reqPosRows);
        columns.negativeImplicit =
            (uint16_t)(columns.negativeImplicit + reqNegCols);
        columns.positiveImplicit =
            (uint16_t)(columns.positiveImplicit + reqPosCols);
    }

    void MarkAreaAs(AbsoluteAxis primaryAxis, LineOzl primarySpan,
                    LineOzl secondarySpan, CellOccupancyState value) {
        LineOzl rowSpan = primaryAxis == AbsoluteAxis::Horizontal
                              ? secondarySpan
                              : primarySpan;
        LineOzl colSpan = primaryAxis == AbsoluteAxis::Horizontal
                              ? primarySpan
                              : secondarySpan;

        int colStart = columns.OzLineToNextTrack(colSpan.start);
        int colEnd = columns.OzLineToNextTrack(colSpan.end);
        int rowStart = rows.OzLineToNextTrack(rowSpan.start);
        int rowEnd = rows.OzLineToNextTrack(rowSpan.end);

        if (!IsAreaInRange(AbsoluteAxis::Horizontal, colStart, colEnd, rowStart,
                           rowEnd)) {
            ExpandToFitRange(rowStart, rowEnd, colStart, colEnd);
            colStart = columns.OzLineToNextTrack(colSpan.start);
            colEnd = columns.OzLineToNextTrack(colSpan.end);
            rowStart = rows.OzLineToNextTrack(rowSpan.start);
            rowEnd = rows.OzLineToNextTrack(rowSpan.end);
        }

        for (int x = rowStart; x < rowEnd; x++) {
            for (int y = colStart; y < colEnd; y++) {
                Set(x, y, value);
            }
        }
    }

    bool TrackAreaIsUnoccupied(AbsoluteAxis primaryAxis, int primaryStart,
                               int primaryEnd, int secondaryStart,
                               int secondaryEnd) const {
        int rowStart = primaryAxis == AbsoluteAxis::Horizontal ? secondaryStart
                                                               : primaryStart;
        int rowEnd =
            primaryAxis == AbsoluteAxis::Horizontal ? secondaryEnd : primaryEnd;
        int colStart = primaryAxis == AbsoluteAxis::Horizontal ? primaryStart
                                                               : secondaryStart;
        int colEnd =
            primaryAxis == AbsoluteAxis::Horizontal ? primaryEnd : secondaryEnd;
        // Out-of-bounds cells count as unoccupied.
        for (int x = rowStart; x < rowEnd; x++) {
            for (int y = colStart; y < colEnd; y++) {
                if (Get(x, y) != CellOccupancyState::Unoccupied) {
                    return false;
                }
            }
        }
        return true;
    }

    bool LineAreaIsUnoccupied(AbsoluteAxis primaryAxis, LineOzl primarySpan,
                              LineOzl secondarySpan) const {
        const TrackCounts& pc = Counts(primaryAxis);
        const TrackCounts& sc = Counts(OtherAxis(primaryAxis));
        return TrackAreaIsUnoccupied(primaryAxis,
                                     pc.OzLineToNextTrack(primarySpan.start),
                                     pc.OzLineToNextTrack(primarySpan.end),
                                     sc.OzLineToNextTrack(secondarySpan.start),
                                     sc.OzLineToNextTrack(secondarySpan.end));
    }

    bool RowIsOccupied(int rowIndex) const {
        if (rowIndex < 0 || rowIndex >= nRows) {
            return false;
        }
        for (int c = 0; c < nCols; c++) {
            if (Get(rowIndex, c) != CellOccupancyState::Unoccupied) {
                return true;
            }
        }
        return false;
    }
    bool ColumnIsOccupied(int columnIndex) const {
        if (columnIndex < 0 || columnIndex >= nCols) {
            return false;
        }
        for (int r = 0; r < nRows; r++) {
            if (Get(r, columnIndex) != CellOccupancyState::Unoccupied) {
                return true;
            }
        }
        return false;
    }

    // The last cell of the given state in the track, searching backwards.
    OptOriginZeroLine LastOfType(AbsoluteAxis trackType, OriginZeroLine startAt,
                                 CellOccupancyState kind) const {
        const TrackCounts& tc = Counts(OtherAxis(trackType));
        int idx = tc.OzLineToNextTrack(startAt);
        if (trackType == AbsoluteAxis::Horizontal) {
            if (idx < 0 || idx >= nRows) {
                return OptOriginZeroLine();
            }
            for (int c = nCols - 1; c >= 0; c--) {
                if (Get(idx, c) == kind) {
                    return OptOriginZeroLine(tc.TrackToPrevOzLine((uint16_t)c));
                }
            }
        } else {
            if (idx < 0 || idx >= nCols) {
                return OptOriginZeroLine();
            }
            for (int r = nRows - 1; r >= 0; r--) {
                if (Get(r, idx) == kind) {
                    return OptOriginZeroLine(tc.TrackToPrevOzLine((uint16_t)r));
                }
            }
        }
        return OptOriginZeroLine();
    }

    // The first cell of the given state in the track, searching forwards.
    OptOriginZeroLine FirstOfType(AbsoluteAxis trackType,
                                  OriginZeroLine startAt,
                                  CellOccupancyState kind) const {
        const TrackCounts& tc = Counts(OtherAxis(trackType));
        int idx = tc.OzLineToNextTrack(startAt);
        if (trackType == AbsoluteAxis::Horizontal) {
            if (idx < 0 || idx >= nRows) {
                return OptOriginZeroLine();
            }
            for (int c = 0; c < nCols; c++) {
                if (Get(idx, c) == kind) {
                    return OptOriginZeroLine(tc.TrackToPrevOzLine((uint16_t)c));
                }
            }
        } else {
            if (idx < 0 || idx >= nCols) {
                return OptOriginZeroLine();
            }
            for (int r = 0; r < nRows; r++) {
                if (Get(r, idx) == kind) {
                    return OptOriginZeroLine(tc.TrackToPrevOzLine((uint16_t)r));
                }
            }
        }
        return OptOriginZeroLine();
    }
};

// ─── types/grid_item.rs ──────────────────────────────────────────────────

struct GridItem {
    NodeId node;
    // The index of the item among the children. Track sizing re-sorts the
    // list; this puts it back for final positioning.
    uint16_t sourceOrder = 0;

    // The definite placement the placement algorithm resolved, in OriginZero
    // coordinates.
    LineOzl row;
    LineOzl column;

    bool isCompressibleReplaced = false;
    PointOverflow overflow;
    BoxSizing boxSizing = BoxSizing::BorderBox;
    SizeDim size;
    SizeDim minSize;
    SizeDim maxSize;
    Optf aspectRatio;
    RectLp padding;
    RectLp border;
    RectLpa margin;
    AlignSelf alignSelf;
    AlignSelf justifySelf;
    Optf baseline;
    // Acts like an extra top margin, for baseline alignment.
    float baselineShim = 0.0f;

    // The same placement as GridTrackVec indices.
    LineU16 rowIndexes;
    LineU16 columnIndexes;

    bool crossesFlexibleRow = false;
    bool crossesFlexibleColumn = false;
    bool crossesIntrinsicRow = false;
    bool crossesIntrinsicColumn = false;

    // Caches, valid for a single run of the track-sizing algorithm.
    SizeOptF gridAreaSizeCache;
    bool hasGridAreaSizeCache = false;
    SizeOptF minContentContributionCache;
    SizeOptF minimumContributionCache;
    SizeOptF maxContentContributionCache;

    // Final position and height, for the container's baseline.
    float yPosition = 0.0f;
    float height = 0.0f;

    LineOzl Placement(AbstractAxis axis) const {
        return axis == AbstractAxis::Block ? row : column;
    }
    LineU16 PlacementIndexes(AbstractAxis axis) const {
        return axis == AbstractAxis::Block ? rowIndexes : columnIndexes;
    }
    // The GridTrackVec range covering the tracks and lines this item spans,
    // excluding the lines that bound it.
    int TrackRangeStart(AbstractAxis axis) const {
        return (int)PlacementIndexes(axis).start + 1;
    }
    int TrackRangeEnd(AbstractAxis axis) const {
        return (int)PlacementIndexes(axis).end;
    }
    uint16_t Span(AbstractAxis axis) const { return Placement(axis).Span(); }
    bool CrossesFlexibleTrack(AbstractAxis axis) const {
        return axis == AbstractAxis::Inline ? crossesFlexibleColumn
                                            : crossesFlexibleRow;
    }
    bool CrossesIntrinsicTrack(AbstractAxis axis) const {
        return axis == AbstractAxis::Inline ? crossesIntrinsicColumn
                                            : crossesIntrinsicRow;
    }
};

// ─── stable sort ─────────────────────────────────────────────────────────
//
// Rust's `sort_by` and `sort_by_key` are stable, and grid placement depends on
// that: two items with the same sort key have to stay in document order.
//
// This is a bottom-up merge sort over insertion-sorted runs, which is the
// shape of Rust's own `slice::sort`. It began as a plain insertion sort, on
// the reasoning that a grid's item list is short — which is true of a grid
// someone wrote by hand and false of the ones the benchmarks build. A 316x316
// grid has 99,856 items, and sorting them by hand took 94 seconds of the
// benchmark's 95.

template <typename T, typename Less>
static void InsertionSortRange(T* items, int lo, int hi, Less less) {
    for (int i = lo + 1; i < hi; i++) {
        T key = items[i];
        int j = i - 1;
        while (j >= lo && less(key, items[j])) {
            items[j + 1] = items[j];
            j--;
        }
        items[j + 1] = key;
    }
}

// `!less(right, left)` rather than `less(left, right)` is what keeps equal
// elements in their original order.
template <typename T, typename Less>
static void MergeRuns(const T* src, T* dst, int lo, int mid, int hi,
                      Less less) {
    int i = lo;
    int j = mid;
    int k = lo;
    while (i < mid && j < hi) {
        if (!less(src[j], src[i])) {
            dst[k++] = src[i++];
        } else {
            dst[k++] = src[j++];
        }
    }
    while (i < mid) {
        dst[k++] = src[i++];
    }
    while (j < hi) {
        dst[k++] = src[j++];
    }
}

template <typename T, typename Less>
void StableSort(T* items, int n, Less less) {
    if (n < 2) {
        return;
    }
    // Short runs are insertion sorted, then merged pairwise. Below the
    // threshold there is nothing to merge.
    const int kRun = 32;
    for (int lo = 0; lo < n; lo += kRun) {
        int hi = lo + kRun < n ? lo + kRun : n;
        InsertionSortRange(items, lo, hi, less);
    }
    if (n <= kRun) {
        return;
    }

    T* scratch = (T*)base::Alloc(nullptr, n * (int)sizeof(T));
    if (!scratch) {
        // Out of memory for the scratch half. The insertion sort still sorts,
        // just slowly, and a sorted list is what the caller needs.
        InsertionSortRange(items, 0, n, less);
        return;
    }

    T* src = items;
    T* dst = scratch;
    for (int width = kRun; width < n; width *= 2) {
        for (int lo = 0; lo < n; lo += 2 * width) {
            int mid = lo + width < n ? lo + width : n;
            int hi = lo + 2 * width < n ? lo + 2 * width : n;
            MergeRuns(src, dst, lo, mid, hi, less);
        }
        T* swap = src;
        src = dst;
        dst = swap;
    }
    if (src != items) {
        memcpy((void*)items, (const void*)src, (size_t)n * sizeof(T));
    }
    base::Free(nullptr, (void*)scratch);
}

// ─── types/named.rs ──────────────────────────────────────────────────────
//
// Rust builds the strings "{name}-start" and "{name}-end" to key its maps.
// Here the suffix is a field, so a lookup composes the comparison instead of
// the string.

enum class NameSuffix : uint8_t {
    None,
    Start,
    End
};

struct LineNameEntry {
    Str name;
    NameSuffix suffix = NameSuffix::None;
    Vec<uint16_t> lines;
};

bool StrEqLen(Str a, Str b) {
    return a.len == b.len &&
           (a.len == 0 || memcmp(a.s, b.s, (size_t)a.len) == 0);
}

// Resolves named grid lines and areas into line numbers.
struct NamedLineResolver {
    Vec<LineNameEntry> rowLines;
    Vec<LineNameEntry> columnLines;
    Slice<GridTemplateArea> areas;
    uint16_t areaColumnCount = 0;
    uint16_t areaRowCount = 0;
    uint16_t explicitColumnCount = 0;
    uint16_t explicitRowCount = 0;

    void Free() {
        for (int i = 0; i < rowLines.len; i++) {
            rowLines[i].lines.Reset();
        }
        for (int i = 0; i < columnLines.len; i++) {
            columnLines[i].lines.Reset();
        }
        rowLines.Reset();
        columnLines.Reset();
    }

    static void Upsert(Vec<LineNameEntry>* map, Str name, NameSuffix suffix,
                       uint16_t value) {
        for (int i = 0; i < map->len; i++) {
            LineNameEntry& e = (*map)[i];
            if (e.suffix == suffix && StrEqLen(e.name, name)) {
                for (int k = 0; k < e.lines.len; k++) {
                    if (e.lines[k] == value) {
                        return;
                    }
                }
                e.lines.Append(value);
                return;
            }
        }
        LineNameEntry e;
        e.name = name;
        e.suffix = suffix;
        e.lines.Append(value);
        map->Append(e);
    }

    static const Vec<uint16_t>* Find(const Vec<LineNameEntry>& map, Str name,
                                     NameSuffix suffix) {
        for (int i = 0; i < map.len; i++) {
            const LineNameEntry& e = map[i];
            if (e.suffix == suffix && StrEqLen(e.name, name)) {
                return &e.lines;
            }
        }
        return nullptr;
    }

    void Init(const Style& style, uint16_t columnAutoRepetitions,
              uint16_t rowAutoRepetitions);
    LinePlain ResolveLineNames(LinePlacement line, GridAreaAxis axis) const;
    LinePlain ResolveRowNames(LinePlacement line) const {
        return ResolveLineNames(line, GridAreaAxis::Row);
    }
    LinePlain ResolveColumnNames(LinePlacement line) const {
        return ResolveLineNames(line, GridAreaAxis::Column);
    }
    // Rust takes a `filter_lines` closure; the two callers want either all the
    // lines, those strictly after a bound, or those strictly before one, so
    // the bound comes in as a range instead.
    GridLine FindLineIndex(Str name, int16_t idx, GridAreaAxis axis,
                           GridAreaEnd end, int filterFrom, int filterTo) const;
};

void NamedLineResolver::Init(const Style& style, uint16_t columnAutoRepetitions,
                             uint16_t rowAutoRepetitions) {
    areas = style.gridTemplateAreas;
    for (int i = 0; i < areas.len; i++) {
        const GridTemplateArea& area = areas[i];
        uint16_t colEnd = area.columnEnd > 1 ? area.columnEnd : 1;
        uint16_t rowEnd = area.rowEnd > 1 ? area.rowEnd : 1;
        if ((uint16_t)(colEnd - 1) > areaColumnCount) {
            areaColumnCount = (uint16_t)(colEnd - 1);
        }
        if ((uint16_t)(rowEnd - 1) > areaRowCount) {
            areaRowCount = (uint16_t)(rowEnd - 1);
        }
        Upsert(&columnLines, area.name, NameSuffix::Start, area.columnStart);
        Upsert(&columnLines, area.name, NameSuffix::End, area.columnEnd);
        Upsert(&rowLines, area.name, NameSuffix::Start, area.rowStart);
        Upsert(&rowLines, area.name, NameSuffix::End, area.rowEnd);
    }

    // The line names between the tracks, walked alongside the template so a
    // repeat() contributes its names once per repetition.
    struct Axis {
        Slice<GridTemplateComponent> tracks;
        Slice<LineNameSet> names;
        uint16_t autoRepetitions;
        Vec<LineNameEntry>* map;
    };
    Axis axes[2] = {{style.gridTemplateColumns, style.gridTemplateColumnNames,
                     columnAutoRepetitions, &columnLines},
                    {style.gridTemplateRows, style.gridTemplateRowNames,
                     rowAutoRepetitions, &rowLines}};

    for (const Axis& ax : axes) {
        int currentLine = 0;
        int trackIdx = 0;
        for (int i = 0; i < ax.names.len; i++) {
            currentLine += 1;
            const LineNameSet& set = ax.names[i];
            for (int k = 0; k < set.names.len; k++) {
                Upsert(ax.map, set.names[k], NameSuffix::None,
                       (uint16_t)currentLine);
            }
            if (trackIdx >= ax.tracks.len) {
                continue;
            }
            const GridTemplateComponent& comp = ax.tracks[trackIdx];
            trackIdx++;
            if (!comp.isRepeat) {
                continue;
            }
            uint16_t repeatCount = comp.repeat.count.IsAuto()
                                       ? ax.autoRepetitions
                                       : comp.repeat.count.count;
            for (uint16_t r = 0; r < repeatCount; r++) {
                for (int s = 0; s < comp.repeat.lineNames.len; s++) {
                    const LineNameSet& ls = comp.repeat.lineNames[s];
                    for (int k = 0; k < ls.names.len; k++) {
                        Upsert(ax.map, ls.names[k], NameSuffix::None,
                               (uint16_t)currentLine);
                    }
                    currentLine += 1;
                }
                // The last line name set collapses with the following one.
                currentLine -= 1;
            }
            currentLine -= 1;
        }
    }

    // Rust sorts and dedups each name's line list; Upsert already dedups, and
    // the lines are appended in increasing order, so they are sorted already.
}

GridLine NamedLineResolver::FindLineIndex(Str name, int16_t idx,
                                          GridAreaAxis axis, GridAreaEnd end,
                                          int filterFrom, int filterTo) const {
    int16_t explicitTrackCount = axis == GridAreaAxis::Row
                                     ? (int16_t)explicitRowCount
                                     : (int16_t)explicitColumnCount;
    // An index of 0 means "no index specified".
    if (idx == 0) {
        idx = 1;
    }

    const Vec<LineNameEntry>& lookup =
        axis == GridAreaAxis::Row ? rowLines : columnLines;
    const Vec<uint16_t>* lines = Find(lookup, name, NameSuffix::None);
    if (!lines) {
        lines = Find(
            lookup, name,
            end == GridAreaEnd::Start ? NameSuffix::Start : NameSuffix::End);
    }

    if (lines) {
        int from = filterFrom < 0 ? 0 : filterFrom;
        int to = filterTo < 0 || filterTo > lines->len ? lines->len : filterTo;
        int count = to - from;
        if (count < 0) {
            count = 0;
        }
        int16_t absIdx = idx < 0 ? (int16_t)-idx : idx;
        if (absIdx <= (int16_t)count) {
            if (idx > 0) {
                return GridLine{(int16_t)(*lines)[from + absIdx - 1]};
            }
            return GridLine{(int16_t)(*lines)[from + count - absIdx]};
        }
        int16_t remaining =
            (int16_t)((absIdx - (int16_t)count) * (idx > 0 ? 1 : -1));
        if (idx > 0) {
            return GridLine{(int16_t)((explicitTrackCount + 1) + remaining)};
        }
        return GridLine{(int16_t)(-((explicitTrackCount + 1) + remaining))};
    }

    // CSS Grid matches a non-existent line name to the first positive implicit
    // line. A grid has one more explicit line than it has tracks, and the
    // fallback is the line *after* that.
    // https://github.com/w3c/csswg-drafts/issues/966#issuecomment-277042153
    if (idx > 0) {
        return GridLine{(int16_t)((explicitTrackCount + 1) + idx)};
    }
    return GridLine{(int16_t)(-((explicitTrackCount + 1) + idx))};
}

// The partition point of a sorted line list: how many entries satisfy the
// predicate `line <= bound` (or `line < bound`).
int PartitionPoint(const Vec<uint16_t>* lines, uint16_t bound, bool inclusive) {
    if (!lines) {
        return 0;
    }
    int n = 0;
    for (int i = 0; i < lines->len; i++) {
        bool keep = inclusive ? ((*lines)[i] <= bound) : ((*lines)[i] < bound);
        if (!keep) {
            break;
        }
        n++;
    }
    return n;
}

LinePlain NamedLineResolver::ResolveLineNames(LinePlacement line,
                                              GridAreaAxis axis) const {
    GridPlacement start = line.start;
    GridPlacement end = line.end;

    if (start.kind == GridPlacementKind::NamedLine) {
        start = GridPlacement::FromLineIndex(
            FindLineIndex(start.name, start.line, axis, GridAreaEnd::Start, -1,
                          -1)
                .v);
    }
    if (end.kind == GridPlacementKind::NamedLine) {
        end = GridPlacement::FromLineIndex(
            FindLineIndex(end.name, end.line, axis, GridAreaEnd::End, -1, -1)
                .v);
    }

    int16_t explicitTrackCount = axis == GridAreaAxis::Row
                                     ? (int16_t)explicitRowCount
                                     : (int16_t)explicitColumnCount;
    const Vec<LineNameEntry>& lookup =
        axis == GridAreaAxis::Row ? rowLines : columnLines;

    // A definite line paired with a named span resolves the span against the
    // lines beyond (or before) that line.
    if (start.kind == GridPlacementKind::Line &&
        end.kind == GridPlacementKind::NamedSpan) {
        int16_t normalizedStart =
            start.line > 0
                ? start.line
                : (int16_t)F32Max((float)(explicitTrackCount + 1 + start.line),
                                  0.0f);
        const Vec<uint16_t>* lines = Find(lookup, end.name, NameSuffix::None);
        int point = PartitionPoint(lines, (uint16_t)normalizedStart, true);
        GridLine endLine = FindLineIndex(end.name, (int16_t)end.span, axis,
                                         GridAreaEnd::End, point, -1);
        return {PlainPlacement::AtLine(start.line),
                PlainPlacement::AtLine(endLine.v)};
    }
    if (start.kind == GridPlacementKind::NamedSpan &&
        end.kind == GridPlacementKind::Line) {
        int16_t normalizedEnd =
            end.line > 0
                ? end.line
                : (int16_t)F32Max((float)(explicitTrackCount + 1 + end.line),
                                  0.0f);
        const Vec<uint16_t>* lines = Find(lookup, start.name, NameSuffix::None);
        int point = PartitionPoint(lines, (uint16_t)normalizedEnd, false);
        GridLine startLine = FindLineIndex(start.name, (int16_t)start.span,
                                           axis, GridAreaEnd::Start, 0, point);
        return {PlainPlacement::AtLine(startLine.v),
                PlainPlacement::AtLine(end.line)};
    }

    auto plain = [](const GridPlacement& p) -> PlainPlacement {
        switch (p.kind) {
            case GridPlacementKind::Line:
                return PlainPlacement::AtLine(p.line);
            case GridPlacementKind::Span:
                return PlainPlacement::Spanning(p.span);
            case GridPlacementKind::NamedSpan:
                // A span for a named line with no matching line is a span of 1.
                return PlainPlacement::Spanning(1);
            default:
                return PlainPlacement::Auto();
        }
    };
    return {plain(start), plain(end)};
}

// ─── implicit_grid.rs ────────────────────────────────────────────────────
//
// Not required for spec compliance: an estimate of the grid size, used to
// pre-size the occupancy matrix and as a step in auto-placement.

struct MinMaxSpan {
    OriginZeroLine minLine;
    OriginZeroLine maxLine;
    uint16_t span = 0;
};

// A conservative estimate of the greatest and smallest grid lines one item
// uses, in OriginZero coordinates.
MinMaxSpan ChildMinLineMaxLineSpan(LinePlacement line,
                                   uint16_t explicitTrackCount) {
    // 8.3.1 Grid Placement Conflict Handling:
    //   A. Two lines with the start further end-ward than the end: swap them.
    //   B. Start equal to end: remove the end line.
    //   C. Two spans: remove the one from the end property.
    //   D. Only a span for a named line: replace it with a span of 1.
    //
    // Named lines are accounted for separately, so they are ignored here.
    LinePlain oz = line.IntoOriginZeroIgnoringNamed(explicitTrackCount);
    OriginZeroLine t1 = oz.start.Ozl();
    OriginZeroLine t2 = oz.end.Ozl();

    MinMaxSpan out;
    if (oz.start.IsLine() && oz.end.IsLine()) {
        out.minLine = (t1 == t2) ? t1 : (t1 < t2 ? t1 : t2);
        out.maxLine = (t1 == t2) ? t1 + (uint16_t)1 : (t1 > t2 ? t1 : t2);
    } else if (oz.start.IsLine() && oz.end.IsAuto()) {
        out.minLine = t1;
        out.maxLine = t1 + (uint16_t)1;
    } else if (oz.start.IsLine() && oz.end.IsSpan()) {
        out.minLine = t1;
        out.maxLine = t1 + oz.end.span;
    } else if (oz.start.IsAuto() && oz.end.IsLine()) {
        out.minLine = t2;
        out.maxLine = t2;
    } else if (oz.start.IsSpan() && oz.end.IsLine()) {
        out.minLine = t2 - oz.start.span;
        out.maxLine = t2;
    } else {
        // Only spans or autos: these are accounted for by the span below, so
        // the lines stay at zero and never affect the estimate.
        out.minLine = OriginZeroLine{0};
        out.maxLine = OriginZeroLine{0};
    }

    bool startIndefinite = oz.start.IsAuto() || oz.start.IsSpan();
    bool endIndefinite = oz.end.IsAuto() || oz.end.IsSpan();
    out.span = (startIndefinite && endIndefinite) ? oz.IndefiniteSpan() : 1;
    return out;
}

// One child's grid placement, as the estimate and the placement pass see it.
struct ChildPlacementStyles {
    LinePlacement column;
    LinePlacement row;
};

// Estimate the number of rows and columns in the implicit grid. The explicit
// and negative implicit counts are exact; the positive implicit count is a
// lower bound, since auto-placement can add to it.
void ComputeGridSizeEstimate(uint16_t explicitColCount,
                             uint16_t explicitRowCount, Direction direction,
                             const ChildPlacementStyles* children, int n,
                             TrackCounts* outCols, TrackCounts* outRows) {
    OriginZeroLine colMin{0};
    OriginZeroLine colMax{0};
    uint16_t colMaxSpan = 0;
    OriginZeroLine rowMin{0};
    OriginZeroLine rowMax{0};
    uint16_t rowMaxSpan = 0;

    for (int i = 0; i < n; i++) {
        MinMaxSpan colEst =
            ChildMinLineMaxLineSpan(children[i].column, explicitColCount);
        MinMaxSpan rowEst =
            ChildMinLineMaxLineSpan(children[i].row, explicitRowCount);

        // Placement mirrors horizontal spans in RTL, so the known column
        // bounds are mirrored here too, to keep the pre-sizing consistent.
        if (IsRtl(direction) && (colEst.minLine != OriginZeroLine{0} ||
                                 colEst.maxLine != OriginZeroLine{0})) {
            int16_t endLine = (int16_t)explicitColCount;
            OriginZeroLine mirroredMin{(int16_t)(endLine - colEst.maxLine.v)};
            OriginZeroLine mirroredMax{(int16_t)(endLine - colEst.minLine.v)};
            colEst.minLine = mirroredMin;
            colEst.maxLine = mirroredMax;
        }
        if (colEst.minLine < colMin) {
            colMin = colEst.minLine;
        }
        if (colEst.maxLine > colMax) {
            colMax = colEst.maxLine;
        }
        if (colEst.span > colMaxSpan) {
            colMaxSpan = colEst.span;
        }
        if (rowEst.minLine < rowMin) {
            rowMin = rowEst.minLine;
        }
        if (rowEst.maxLine > rowMax) {
            rowMax = rowEst.maxLine;
        }
        if (rowEst.span > rowMaxSpan) {
            rowMaxSpan = rowEst.span;
        }
    }

    uint16_t negCols = ImpliedNegativeImplicitTracks(colMin);
    uint16_t posCols = ImpliedPositiveImplicitTracks(colMax, explicitColCount);
    uint16_t negRows = ImpliedNegativeImplicitTracks(rowMin);
    uint16_t posRows = ImpliedPositiveImplicitTracks(rowMax, explicitRowCount);

    // An item whose span does not fit in the estimate pushes the positive
    // count out to make room for it.
    if ((uint16_t)(negCols + explicitColCount + posCols) < colMaxSpan) {
        posCols = (uint16_t)(colMaxSpan - explicitColCount - negCols);
    }
    if ((uint16_t)(negRows + explicitRowCount + posRows) < rowMaxSpan) {
        posRows = (uint16_t)(rowMaxSpan - explicitRowCount - negRows);
    }

    *outCols = TrackCounts::FromRaw(negCols, explicitColCount, posCols);
    *outRows = TrackCounts::FromRaw(negRows, explicitRowCount, posRows);
}

// ─── explicit_grid.rs ────────────────────────────────────────────────────

// How many times an auto-repeat should repeat.
enum class AutoRepeatStrategy : uint8_t {
    // The container has a definite size or max size: the largest count that
    // does not overflow.
    MaxRepetitionsThatDoNotOverflow,
    // The container has a definite min size: the smallest count that meets it.
    MinRepetitionsThatDoOverflow
};

// Treat a track as its max sizing function when that is definite, or as its
// min otherwise, flooring the max by the min when both are definite.
float TrackDefiniteValue(TrackSizingFunction fn, Optf parentSize,
                         CalcResolver calc) {
    Optf maxSize = fn.max.DefiniteValue(parentSize, calc);
    Optf minSize = fn.min.DefiniteValue(parentSize, calc);
    if (maxSize.IsSome()) {
        return MaybeMax(maxSize.val, minSize);
    }
    return minSize.UnwrapOr(0.0f);
}

struct ExplicitGridSize {
    uint16_t autoRepetitionCount = 0;
    uint16_t trackCount = 0;
};

ExplicitGridSize ComputeExplicitGridSizeInAxis(
    const Style& style, Optf autoFitContainerSize,
    AutoRepeatStrategy autoFitStrategy, CalcResolver calc, AbsoluteAxis axis) {
    Slice<GridTemplateComponent> templ = axis == AbsoluteAxis::Horizontal
                                             ? style.gridTemplateColumns
                                             : style.gridTemplateRows;
    if (templ.len == 0) {
        return {};
    }

    // A repetition with no tracks makes the whole definition invalid.
    for (int i = 0; i < templ.len; i++) {
        if (templ[i].isRepeat && templ[i].repeat.TrackCount() == 0) {
            return {};
        }
    }

    uint16_t nonAutoRepeatingTrackCount = 0;
    uint16_t autoRepetitionCount = 0;
    bool allTrackDefsHaveFixedComponent = true;
    for (int i = 0; i < templ.len; i++) {
        const GridTemplateComponent& c = templ[i];
        if (!c.isRepeat) {
            nonAutoRepeatingTrackCount += 1;
            if (!c.single.HasFixedComponent()) {
                allTrackDefsHaveFixedComponent = false;
            }
            continue;
        }
        if (!c.repeat.count.IsAuto()) {
            nonAutoRepeatingTrackCount =
                (uint16_t)(nonAutoRepeatingTrackCount +
                           c.repeat.count.count * c.repeat.TrackCount());
        } else {
            autoRepetitionCount += 1;
        }
        for (int k = 0; k < c.repeat.tracks.len; k++) {
            if (!c.repeat.tracks[k].HasFixedComponent()) {
                allTrackDefsHaveFixedComponent = false;
            }
        }
    }

    // More than one auto-repetition, or one mixed with non-fixed track sizing
    // functions, makes the template invalid: fall back to no explicit tracks.
    bool templateIsValid =
        autoRepetitionCount == 0 ||
        (autoRepetitionCount == 1 && allTrackDefsHaveFixedComponent);
    if (!templateIsValid) {
        return {};
    }

    if (autoRepetitionCount == 0) {
        return {0, nonAutoRepeatingTrackCount};
    }

    const GridTemplateRepetition* repetition = nullptr;
    for (int i = 0; i < templ.len; i++) {
        if (templ[i].isRepeat && templ[i].repeat.count.IsAuto()) {
            repetition = &templ[i].repeat;
            break;
        }
    }
    uint16_t repetitionTrackCount = repetition->TrackCount();

    uint16_t numRepetitions = 1;
    if (autoFitContainerSize.IsSome()) {
        float innerContainerSize = autoFitContainerSize.val;
        Optf parentSize = Optf(innerContainerSize);

        float nonRepeatingTrackUsedSpace = 0.0f;
        for (int i = 0; i < templ.len; i++) {
            const GridTemplateComponent& c = templ[i];
            if (!c.isRepeat) {
                nonRepeatingTrackUsedSpace +=
                    TrackDefiniteValue(c.single, parentSize, calc);
                continue;
            }
            if (c.repeat.count.IsAuto()) {
                continue;
            }
            float sum = 0.0f;
            for (int k = 0; k < c.repeat.tracks.len; k++) {
                sum += TrackDefiniteValue(c.repeat.tracks[k], parentSize, calc);
            }
            nonRepeatingTrackUsedSpace += sum * (float)c.repeat.count.count;
        }

        SizeLp gapStyle = style.gap;
        LengthPercentage gapLp = gapStyle.GetAbs(axis);
        SizeLp asSize = {gapLp, gapLp};
        float gapSize = asSize.ResolveOrZero(Optf(innerContainerSize), calc)
                            .width;

        float perRepetitionTrackUsedSpace = 0.0f;
        for (int k = 0; k < repetition->tracks.len; k++) {
            perRepetitionTrackUsedSpace +=
                TrackDefiniteValue(repetition->tracks[k], parentSize, calc);
        }

        // The first repetition is special-cased: how many gaps it contributes
        // depends on how many non-repeating tracks the template has.
        int gapCount =
            (int)nonAutoRepeatingTrackCount + (int)repetitionTrackCount - 1;
        if (gapCount < 0) {
            gapCount = 0;
        }
        float firstRepetitionAndNonRepeatingTracksUsedSpace =
            nonRepeatingTrackUsedSpace + perRepetitionTrackUsedSpace +
            (float)gapCount * gapSize;

        if (firstRepetitionAndNonRepeatingTracksUsedSpace >
            innerContainerSize) {
            // A single repetition already overflows; the count floors at one.
            numRepetitions = 1;
        } else {
            float perRepetitionGapUsedSpace =
                (float)repetitionTrackCount * gapSize;
            float perRepetitionUsedSpace =
                perRepetitionTrackUsedSpace + perRepetitionGapUsedSpace;
            float numRepetitionThatFit =
                (innerContainerSize -
                 firstRepetitionAndNonRepeatingTracksUsedSpace) /
                perRepetitionUsedSpace;
            // Plus the repetition already accounted for above.
            numRepetitions =
                autoFitStrategy ==
                        AutoRepeatStrategy::MaxRepetitionsThatDoNotOverflow
                    ? (uint16_t)((int)floorf(numRepetitionThatFit) + 1)
                    : (uint16_t)((int)ceilf(numRepetitionThatFit) + 1);
        }
    }

    uint16_t gridTemplateTrackCount =
        (uint16_t)(nonAutoRepeatingTrackCount +
                   repetitionTrackCount * numRepetitions);
    return {numRepetitions, gridTemplateTrackCount};
}

template <typename NextTrack>
void CreateImplicitTracks(Vec<GridTrack>* tracks, uint16_t count,
                          NextTrack nextTrack, LengthPercentage gap) {
    for (uint16_t i = 0; i < count; i++) {
        TrackSizingFunction def = nextTrack();
        tracks->Append(
            GridTrack::New(def.MinSizingFunction(), def.MaxSizingFunction()));
        tracks->Append(GridTrack::Gutter(gap));
    }
}

template <typename TrackHasItems>
void InitializeGridTracks(Vec<GridTrack>* tracks, TrackCounts counts,
                          const Style& style, AbsoluteAxis axis,
                          TrackHasItems trackHasItems) {
    Slice<GridTemplateComponent> trackTemplate;
    Slice<TrackSizingFunction> autoTracks;
    LengthPercentage gap;
    if (axis == AbsoluteAxis::Horizontal) {
        trackTemplate = style.gridTemplateColumns;
        autoTracks = style.gridAutoColumns;
        gap = style.gap.width;
    } else {
        trackTemplate = style.gridTemplateRows;
        autoTracks = style.gridAutoRows;
        gap = style.gap.height;
    }

    tracks->len = 0;
    tracks->Append(GridTrack::Gutter(gap));

    int autoTrackCount = autoTracks.len;
    uint16_t nonAutoRepeatingTrackCount = 0;
    for (int i = 0; i < trackTemplate.len; i++) {
        const GridTemplateComponent& c = trackTemplate[i];
        if (!c.isRepeat) {
            nonAutoRepeatingTrackCount += 1;
        } else if (!c.repeat.count.IsAuto()) {
            nonAutoRepeatingTrackCount =
                (uint16_t)(nonAutoRepeatingTrackCount +
                           c.repeat.count.count * c.repeat.TrackCount());
        }
    }

    // Negative implicit tracks. Rust cycles the grid-auto-* list backwards
    // from the explicit grid, which is what the offset below does.
    if (counts.negativeImplicit > 0) {
        if (autoTrackCount == 0) {
            CreateImplicitTracks(
                tracks, counts.negativeImplicit,
                []() { return TrackSizingFunction::Auto(); }, gap);
        } else {
            int offset = autoTrackCount -
                         ((int)counts.negativeImplicit % autoTrackCount);
            int cursor = offset;
            CreateImplicitTracks(
                tracks, counts.negativeImplicit,
                [&]() {
                    TrackSizingFunction t = autoTracks[cursor % autoTrackCount];
                    cursor++;
                    return t;
                },
                gap);
        }
    }

    int currentTrackIndex = (int)counts.negativeImplicit;

    // Explicit tracks. The count is checked rather than the template being
    // empty, because a count of zero can come from an invalid template.
    if (counts.explicitCount > 0) {
        for (int i = 0; i < trackTemplate.len; i++) {
            const GridTemplateComponent& c = trackTemplate[i];
            if (!c.isRepeat) {
                tracks->Append(GridTrack::New(c.single.MinSizingFunction(),
                                              c.single.MaxSizingFunction()));
                tracks->Append(GridTrack::Gutter(gap));
                currentTrackIndex += 1;
                continue;
            }
            if (!c.repeat.count.IsAuto()) {
                int total = (int)c.repeat.TrackCount() * (int)c.repeat.count
                                                             .count;
                for (int k = 0; k < total; k++) {
                    TrackSizingFunction f =
                        c.repeat.tracks[k % c.repeat.tracks.len];
                    tracks->Append(GridTrack::New(f.MinSizingFunction(),
                                                  f.MaxSizingFunction()));
                    tracks->Append(GridTrack::Gutter(gap));
                    currentTrackIndex += 1;
                }
                continue;
            }
            int autoRepeatedTrackCount =
                (int)counts.explicitCount - (int)nonAutoRepeatingTrackCount;
            bool isAutoFit = c.repeat.count
                                 .kind == RepetitionCount::Kind::AutoFit;
            for (int k = 0; k < autoRepeatedTrackCount; k++) {
                TrackSizingFunction def = c.repeat
                                              .tracks[k % c.repeat.tracks.len];
                GridTrack track = GridTrack::New(def.MinSizingFunction(),
                                                 def.MaxSizingFunction());
                GridTrack gutter = GridTrack::Gutter(gap);
                // An auto-fit track with no items in it collapses.
                if (isAutoFit && !trackHasItems(currentTrackIndex)) {
                    track.Collapse();
                    gutter.Collapse();
                }
                tracks->Append(track);
                tracks->Append(gutter);
                currentTrackIndex += 1;
            }

            // Collapsing an auto-fit track collapses the gutter after it but
            // not the one before, which is right so long as a non-collapsed
            // track follows somewhere. When the auto-repeat is the last thing
            // in the list there is none, so walk back to the first
            // non-collapsed track and collapse everything after it.
            bool isLast = currentTrackIndex == counts.Len();
            if (isAutoFit && isLast) {
                for (int t = tracks->len - 1; t >= 0; t--) {
                    GridTrack& prev = (*tracks)[t];
                    if (prev.kind == GridTrackKind::Track &&
                        !prev.isCollapsed) {
                        break;
                    }
                    prev.Collapse();
                }
            }
        }
    }

    int gridAreaTracks = (int)counts.negativeImplicit +
                         (int)counts.explicitCount - currentTrackIndex;
    if (gridAreaTracks < 0) {
        gridAreaTracks = 0;
    }
    uint16_t positive =
        (uint16_t)((int)counts.positiveImplicit + gridAreaTracks);
    if (autoTrackCount == 0) {
        CreateImplicitTracks(
            tracks, positive, []() { return TrackSizingFunction::Auto(); },
            gap);
    } else {
        int cursor = 0;
        CreateImplicitTracks(
            tracks, positive,
            [&]() {
                TrackSizingFunction t = autoTracks[cursor % autoTrackCount];
                cursor++;
                return t;
            },
            gap);
    }

    // The first and last grid lines are collapsed.
    if (tracks->len > 0) {
        (*tracks)[0].Collapse();
        (*tracks)[tracks->len - 1].Collapse();
    }
}

// ─── types/grid_item.rs, the parts that need the tree ────────────────────

// Which estimate of an other-axis track's size the child sizing functions are
// given. Rust passes a closure; there are exactly two of them.
enum class TrackSizeEstimate : uint8_t {
    // `track.max_track_sizing_function.definite_value(parent_size)`
    MaxTrackSizingFunction,
    // `Some(track.base_size)`
    BaseSize
};

Optf EstimateTrackSize(const GridTrack& track, Optf basis,
                       TrackSizeEstimate kind, CalcResolver calc) {
    if (kind == TrackSizeEstimate::BaseSize) {
        return Optf(track.baseSize);
    }
    return track.maxTrackSizingFunction.DefiniteValue(basis, calc);
}

// The item's resolved margins for size contributions. A horizontal percentage
// margin always resolves to zero when the container size is indefinite, since
// otherwise it would be a cyclic dependency.
SizeF MarginsAxisSumsWithBaselineShims(const GridItem& item,
                                       Optf innerNodeWidth, CalcResolver calc) {
    RectLp zeroBasis;
    (void)zeroBasis;
    RectLpa m = item.margin;
    RectF r;
    // Rust resolves left/right against Some(0.0) and top/bottom against the
    // inner node width. That is not a typo: horizontal percentage margins are
    // deliberately zeroed.
    RectLpa horizontalOnly = {m.left, m.right, LengthPercentageAuto::Zero(),
                              LengthPercentageAuto::Zero()};
    RectF h = horizontalOnly.ResolveOrZero(Optf(0.0f), calc);
    RectLpa verticalOnly = {LengthPercentageAuto::Zero(),
                            LengthPercentageAuto::Zero(), m.top, m.bottom};
    RectF v = verticalOnly.ResolveOrZero(innerNodeWidth, calc);
    r.left = h.left;
    r.right = h.right;
    r.top = v.top + item.baselineShim;
    r.bottom = v.bottom;
    return r.SumAxes();
}

// For an item spanning several tracks, the upper limit on its limited min-/
// max-content contribution: the sum of the fixed max sizing functions of the
// tracks it spans, applied only when it spans nothing but such tracks.
Optf SpannedTrackLimit(const GridItem& item, AbstractAxis axis,
                       const GridTrack* axisTracks, Optf axisParentSize,
                       CalcResolver calc) {
    int from = item.TrackRangeStart(axis);
    int to = item.TrackRangeEnd(axis);
    float limit = 0.0f;
    for (int i = from; i < to; i++) {
        Optf v = axisTracks[i]
                     .maxTrackSizingFunction
                     .DefiniteLimit(axisParentSize, calc);
        if (!v.IsSome()) {
            return Optf();
        }
        limit += v.val;
    }
    return Optf(limit);
}

// The same, but excluding fit-content() arguments. Clamps automatic minimum
// contributions.
Optf SpannedFixedTrackLimit(const GridItem& item, AbstractAxis axis,
                            const GridTrack* axisTracks, Optf axisParentSize,
                            CalcResolver calc) {
    int from = item.TrackRangeStart(axis);
    int to = item.TrackRangeEnd(axis);
    float limit = 0.0f;
    for (int i = from; i < to; i++) {
        Optf v = axisTracks[i]
                     .maxTrackSizingFunction
                     .DefiniteValue(axisParentSize, calc);
        if (!v.IsSome()) {
            return Optf();
        }
        limit += v.val;
    }
    return Optf(limit);
}

// The known_dimensions handed to the child sizing functions. The point of this
// is applying stretch alignment, which is what lets percentage sizes further
// down the tree resolve.
SizeOptF ItemKnownDimensions(const GridItem& item, TaffyTree* tree,
                             SizeOptF gridAreaSize) {
    CalcResolver calc = tree->calc;
    SizeF margins =
        MarginsAxisSumsWithBaselineShims(item, gridAreaSize.width, calc);
    Optf aspectRatio = item.aspectRatio;

    // CSS resolves percentage padding and border against the inline size of
    // the containing block, which for a grid item under intrinsic measurement
    // is the grid area's width when that is definite.
    RectF padding = item.padding.ResolveOrZero(gridAreaSize.width, calc);
    RectF border = item.border.ResolveOrZero(gridAreaSize.width, calc);
    SizeF paddingBorderSize = (padding + border).SumAxes();
    SizeF boxSizingAdjustment = item.boxSizing == BoxSizing::ContentBox
                                    ? paddingBorderSize
                                    : SizeF::Zero();
    SizeOptF inherentSize = MaybeAdd(item.size.MaybeResolve(gridAreaSize, calc)
                                         .MaybeApplyAspectRatio(aspectRatio),
                                     boxSizingAdjustment);
    SizeOptF minSize = MaybeAdd(item.minSize.MaybeResolve(gridAreaSize, calc)
                                    .MaybeApplyAspectRatio(aspectRatio),
                                boxSizingAdjustment);
    SizeOptF maxSize = MaybeAdd(item.maxSize.MaybeResolve(gridAreaSize, calc)
                                    .MaybeApplyAspectRatio(aspectRatio),
                                boxSizingAdjustment);

    SizeOptF gridAreaMinusItemMargins = MaybeSub(gridAreaSize, margins);

    Optf width = inherentSize.width;
    if (!width.IsSome() && !item.margin.left.IsAuto() &&
        !item.margin.right.IsAuto() &&
        item.justifySelf.keyword == AlignItemsKeyword::Stretch) {
        width = gridAreaMinusItemMargins.width;
    }
    SizeOptF sized = SizeOptF{width, inherentSize.height}
                         .MaybeApplyAspectRatio(aspectRatio);

    Optf height = sized.height;
    if (!height.IsSome() && !item.margin.top.IsAuto() &&
        !item.margin.bottom.IsAuto() &&
        item.alignSelf.keyword == AlignItemsKeyword::Stretch) {
        height = gridAreaMinusItemMargins.height;
    }
    sized = SizeOptF{sized.width, height}.MaybeApplyAspectRatio(aspectRatio);
    return MaybeClamp(sized, minSize, maxSize);
}

// The grid area's size in the given axis when every spanned track is definite.
// Percentages on a grid item resolve against the grid area, not the container,
// so the area has to stay indefinite until its tracks are.
SizeOptF ItemGridAreaSize(const GridItem& item, AbstractAxis axis,
                          const GridTrack* axisTracks,
                          const GridTrack* otherAxisTracks,
                          SizeOptF availableSpace, TrackSizeEstimate estimate,
                          CalcResolver calc) {
    SizeOptF size;

    {
        float sum = 0.0f;
        bool definite = true;
        int from = item.TrackRangeStart(axis);
        int to = item.TrackRangeEnd(axis);
        for (int i = from; i < to; i++) {
            const GridTrack& t = axisTracks[i];
            Optf mn = t.minTrackSizingFunction
                          .DefiniteValue(availableSpace.Get(axis), calc);
            Optf mx = t.maxTrackSizingFunction
                          .DefiniteValue(availableSpace.Get(axis), calc);
            if (!mn.IsSome() || !mx.IsSome() || mn.val != mx.val) {
                definite = false;
                break;
            }
            sum += t.baseSize;
        }
        size.Set(axis, definite ? Optf(sum) : Optf());
    }

    {
        AbstractAxis other = Other(axis);
        float sum = 0.0f;
        bool definite = true;
        int from = item.TrackRangeStart(other);
        int to = item.TrackRangeEnd(other);
        for (int i = from; i < to; i++) {
            const GridTrack& t = otherAxisTracks[i];
            Optf v =
                EstimateTrackSize(t, availableSpace.Get(other), estimate, calc);
            if (!v.IsSome()) {
                definite = false;
                break;
            }
            sum += v.val + t.contentAlignmentAdjustment;
        }
        size.Set(other, definite ? Optf(sum) : Optf());
    }

    return size;
}

SizeOptF ItemGridAreaSizeCached(GridItem* item, AbstractAxis axis,
                                const GridTrack* axisTracks,
                                const GridTrack* otherAxisTracks,
                                SizeOptF availableSpace,
                                TrackSizeEstimate estimate, CalcResolver calc) {
    if (item->hasGridAreaSizeCache) {
        return item->gridAreaSizeCache;
    }
    SizeOptF s = ItemGridAreaSize(*item, axis, axisTracks, otherAxisTracks,
                                  availableSpace, estimate, calc);
    item->gridAreaSizeCache = s;
    item->hasGridAreaSizeCache = true;
    return s;
}

float ItemMinContentContribution(const GridItem& item, AbstractAxis axis,
                                 TaffyTree* tree, SizeOptF gridAreaSize,
                                 SizeOptF availableSpace) {
    SizeOptF knownDimensions = ItemKnownDimensions(item, tree, gridAreaSize);
    // The child sees the grid area as its containing block during intrinsic
    // measurement, so percentage box properties resolve against it.
    SizeAvail avail = {availableSpace.width.IsSome()
                           ? AvailableSpace::Definite(availableSpace.width.val)
                           : AvailableSpace::MinContent(),
                       availableSpace.height.IsSome()
                           ? AvailableSpace::Definite(availableSpace.height.val)
                           : AvailableSpace::MinContent()};
    return tree->MeasureChildSize(item.node, knownDimensions, gridAreaSize,
                                  avail, SizingMode::InherentSize,
                                  AsAbsNaive(axis), LineBool::False());
}

float ItemMinContentContributionCached(GridItem* item, AbstractAxis axis,
                                       TaffyTree* tree, SizeOptF gridAreaSize,
                                       SizeOptF availableSpace) {
    Optf cached = item->minContentContributionCache.Get(axis);
    if (cached.IsSome()) {
        return cached.val;
    }
    float size = ItemMinContentContribution(*item, axis, tree, gridAreaSize,
                                            availableSpace);
    item->minContentContributionCache.Set(axis, Optf(size));
    return size;
}

float ItemMaxContentContribution(const GridItem& item, AbstractAxis axis,
                                 TaffyTree* tree, SizeOptF gridAreaSize,
                                 SizeOptF availableSpace) {
    SizeOptF knownDimensions = ItemKnownDimensions(item, tree, gridAreaSize);
    SizeAvail avail = {availableSpace.width.IsSome()
                           ? AvailableSpace::Definite(availableSpace.width.val)
                           : AvailableSpace::MaxContent(),
                       availableSpace.height.IsSome()
                           ? AvailableSpace::Definite(availableSpace.height.val)
                           : AvailableSpace::MaxContent()};
    return tree->MeasureChildSize(item.node, knownDimensions, gridAreaSize,
                                  avail, SizingMode::InherentSize,
                                  AsAbsNaive(axis), LineBool::False());
}

float ItemMaxContentContributionCached(GridItem* item, AbstractAxis axis,
                                       TaffyTree* tree, SizeOptF gridAreaSize,
                                       SizeOptF availableSpace) {
    Optf cached = item->maxContentContributionCache.Get(axis);
    if (cached.IsSome()) {
        return cached.val;
    }
    float size = ItemMaxContentContribution(*item, axis, tree, gridAreaSize,
                                            availableSpace);
    item->maxContentContributionCache.Set(axis, Optf(size));
    return size;
}

// The smallest outer size an item can have. If its preferred size behaves as
// auto or depends on its containing block, this is the outer size that would
// result from using its minimum size as its preferred size; otherwise it is
// the min-content contribution.
// https://www.w3.org/TR/css-grid-1/#min-size-auto
float ItemMinimumContribution(GridItem* item, TaffyTree* tree,
                              AbstractAxis axis, const GridTrack* axisTracks,
                              int axisTrackCount, SizeOptF gridAreaSize,
                              SizeOptF innerNodeSize) {
    CalcResolver calc = tree->calc;
    RectF padding = item->padding.ResolveOrZero(gridAreaSize.width, calc);
    RectF border = item->border.ResolveOrZero(gridAreaSize.width, calc);
    SizeF paddingBorderSize = (padding + border).SumAxes();
    SizeF boxSizingAdjustment = item->boxSizing == BoxSizing::ContentBox
                                    ? paddingBorderSize
                                    : SizeF::Zero();

    Optf size = MaybeAdd(item->size.MaybeResolve(gridAreaSize, calc)
                             .MaybeApplyAspectRatio(item->aspectRatio),
                         boxSizingAdjustment)
                    .Get(axis);
    if (!size.IsSome()) {
        size = MaybeAdd(item->minSize.MaybeResolve(gridAreaSize, calc)
                            .MaybeApplyAspectRatio(item->aspectRatio),
                        boxSizingAdjustment)
                   .Get(axis);
    }
    if (!size.IsSome()) {
        Overflow o =
            axis == AbstractAxis::Inline ? item->overflow.x : item->overflow.y;
        size = MaybeIntoAutomaticMinSize(o);
    }
    if (!size.IsSome()) {
        // The automatic minimum size is content-based only if the item spans
        // at least one track whose min sizing function is auto, and either it
        // spans one track or none of the tracks it spans is flexible.
        // Otherwise it is zero.
        bool spansAutoMinTrack = false;
        bool spansAFlexibleTrack = false;
        for (int i = 0; i < axisTrackCount; i++) {
            if (axisTracks[i].minTrackSizingFunction.IsAuto()) {
                spansAutoMinTrack = true;
            }
            if (axisTracks[i].maxTrackSizingFunction.IsFr()) {
                spansAFlexibleTrack = true;
            }
        }
        bool onlySpanOneTrack =
            (item->TrackRangeEnd(axis) - item->TrackRangeStart(axis)) == 1;
        bool useContentBasedMinimum =
            spansAutoMinTrack && (onlySpanOneTrack || !spansAFlexibleTrack);

        if (useContentBasedMinimum) {
            float minimumContribution = ItemMinContentContributionCached(
                item, axis, tree, gridAreaSize, gridAreaSize);
            // A compressible replaced element with a definite preferred or
            // maximum size is capped by it, with indefinite percentages
            // resolved against zero.
            if (item->isCompressibleReplaced) {
                Optf pref = item->size.Get(axis).MaybeResolve(Optf(0.0f), calc);
                Optf mx = item->maxSize.Get(axis)
                              .MaybeResolve(Optf(0.0f), calc);
                minimumContribution =
                    MaybeMin(MaybeMin(minimumContribution, pref), mx);
            }
            size = Optf(minimumContribution);
        } else {
            size = Optf(0.0f);
        }
    }

    // In all cases the suggestion is clamped by the maximum size in the axis,
    // when that is definite. A fit-content() argument does not clamp the
    // content-based minimum the way a fixed max sizing function does.
    Optf limit = SpannedFixedTrackLimit(*item, axis, axisTracks,
                                        innerNodeSize.Get(axis), calc);
    return MaybeMin(size.val, limit);
}

float ItemMinimumContributionCached(GridItem* item, TaffyTree* tree,
                                    AbstractAxis axis,
                                    const GridTrack* axisTracks,
                                    int axisTrackCount, SizeOptF gridAreaSize,
                                    SizeOptF innerNodeSize) {
    Optf cached = item->minimumContributionCache.Get(axis);
    if (cached.IsSome()) {
        return cached.val;
    }
    float size =
        ItemMinimumContribution(item, tree, axis, axisTracks, axisTrackCount,
                                gridAreaSize, innerNodeSize);
    item->minimumContributionCache.Set(axis, Optf(size));
    return size;
}

// ─── placement.rs ────────────────────────────────────────────────────────

bool AxisIsReversed(Direction direction, AbsoluteAxis axis) {
    return IsRtl(direction) && axis == AbsoluteAxis::Horizontal;
}

OriginZeroLine AdvancePosition(OriginZeroLine position, bool reversed) {
    return OriginZeroLine{(int16_t)(position.v + (reversed ? -1 : 1))};
}

OriginZeroLine SearchStartLine(OriginZeroLine gridStartLine,
                               OriginZeroLine gridEndLine, bool reversed) {
    return reversed ? gridEndLine - (uint16_t)1 : gridStartLine;
}

LineOzl ResolveIndefiniteGridSpan(OriginZeroLine position, uint16_t span,
                                  bool reversed) {
    if (reversed) {
        return {(position - span) + (uint16_t)1, position + (uint16_t)1};
    }
    return {position, position + span};
}

LineOzl MirrorHorizontalSpan(LineOzl span, uint16_t explicitColCount) {
    int16_t endLine = (int16_t)explicitColCount;
    return {OriginZeroLine{(int16_t)(endLine - span.end.v)},
            OriginZeroLine{(int16_t)(endLine - span.start.v)}};
}

LineOzl MaybeMirrorSpan(LineOzl span, AbsoluteAxis axis, Direction direction,
                        uint16_t explicitColCount) {
    if (axis == AbsoluteAxis::Horizontal && IsRtl(direction)) {
        return MirrorHorizontalSpan(span, explicitColCount);
    }
    return span;
}

// One child, as the placement algorithm sees it.
struct PlacementChild {
    int index = 0;
    NodeId node;
    LinePlain horizontal;
    LinePlain vertical;

    LinePlain Get(AbsoluteAxis a) const {
        return a == AbsoluteAxis::Horizontal ? horizontal : vertical;
    }
};

void RecordGridPlacement(CellOccupancyMatrix* matrix, Vec<GridItem>* items,
                         TaffyTree* tree, NodeId node, int index,
                         AlignItems parentAlignItems,
                         AlignItems parentJustifyItems,
                         AbsoluteAxis primaryAxis, LineOzl primarySpan,
                         LineOzl secondarySpan,
                         CellOccupancyState placementType) {
    matrix->MarkAreaAs(primaryAxis, primarySpan, secondarySpan, placementType);

    LineOzl colSpan =
        primaryAxis == AbsoluteAxis::Horizontal ? primarySpan : secondarySpan;
    LineOzl rowSpan =
        primaryAxis == AbsoluteAxis::Horizontal ? secondarySpan : primarySpan;

    const Style& s = tree->GetStyle(node);
    GridItem item;
    item.node = node;
    item.sourceOrder = (uint16_t)index;
    item.row = rowSpan;
    item.column = colSpan;
    item.isCompressibleReplaced = s.IsCompressibleReplaced();
    item.overflow = s.overflow;
    item.boxSizing = s.boxSizing;
    item.size = s.size;
    item.minSize = s.minSize;
    item.maxSize = s.maxSize;
    item.aspectRatio = s.aspectRatio;
    item.padding = s.padding;
    item.border = s.border;
    item.margin = s.margin;
    item.alignSelf = s.alignSelf.UnwrapOr(parentAlignItems);
    item.justifySelf = s.justifySelf.UnwrapOr(parentJustifyItems);
    items->Append(item);
}

LineOzl PlaceDefiniteGridItemAxis(const PlacementChild& child,
                                  AbsoluteAxis axis, Direction direction,
                                  uint16_t explicitColCount) {
    return MaybeMirrorSpan(child.Get(axis).ResolveDefiniteGridLines(), axis,
                           direction, explicitColCount);
}

struct SpanPair {
    LineOzl primary;
    LineOzl secondary;
};

SpanPair PlaceDefiniteSecondaryAxisItem(const CellOccupancyMatrix& matrix,
                                        const PlacementChild& child,
                                        GridAutoFlow autoFlow,
                                        Direction direction,
                                        uint16_t explicitColCount) {
    AbsoluteAxis primaryAxis = PrimaryAxis(autoFlow);
    AbsoluteAxis secondaryAxis = OtherAxis(primaryAxis);
    bool primaryReversed = AxisIsReversed(direction, primaryAxis);
    OriginZeroLine primaryStart = matrix.Counts(primaryAxis)
                                      .ImplicitStartLine();
    OriginZeroLine primaryEnd = matrix.Counts(primaryAxis).ImplicitEndLine();

    LineOzl secondaryPlacement =
        MaybeMirrorSpan(child.Get(secondaryAxis).ResolveDefiniteGridLines(),
                        secondaryAxis, direction, explicitColCount);

    OriginZeroLine startingPosition;
    if (IsDense(autoFlow)) {
        startingPosition =
            SearchStartLine(primaryStart, primaryEnd, primaryReversed);
    } else {
        OptOriginZeroLine lookup =
            primaryReversed
                ? matrix.FirstOfType(primaryAxis, secondaryPlacement.start,
                                     CellOccupancyState::AutoPlaced)
                : matrix.LastOfType(primaryAxis, secondaryPlacement.start,
                                    CellOccupancyState::AutoPlaced);
        startingPosition =
            lookup.IsSome()
                ? lookup.val
                : SearchStartLine(primaryStart, primaryEnd, primaryReversed);
    }
    uint16_t primarySpanLen = child.Get(primaryAxis).IndefiniteSpan();

    OriginZeroLine position = startingPosition;
    while (true) {
        LineOzl primaryPlacement = ResolveIndefiniteGridSpan(
            position, primarySpanLen, primaryReversed);
        if (matrix.LineAreaIsUnoccupied(primaryAxis, primaryPlacement,
                                        secondaryPlacement)) {
            return {primaryPlacement, secondaryPlacement};
        }
        position = AdvancePosition(position, primaryReversed);
    }
}

SpanPair PlaceIndefinitelyPositionedItem(const CellOccupancyMatrix& matrix,
                                         const PlacementChild& child,
                                         GridAutoFlow autoFlow,
                                         OriginZeroLine gridPrimary,
                                         OriginZeroLine gridSecondary,
                                         Direction direction,
                                         uint16_t explicitColCount) {
    AbsoluteAxis primaryAxis = PrimaryAxis(autoFlow);
    AbsoluteAxis secondaryAxis = OtherAxis(primaryAxis);
    bool primaryReversed = AxisIsReversed(direction, primaryAxis);
    bool secondaryReversed = AxisIsReversed(direction, secondaryAxis);

    LinePlain primaryStyle = child.Get(primaryAxis);
    LinePlain secondaryStyle = child.Get(secondaryAxis);

    uint16_t secondarySpanLen = secondaryStyle.IndefiniteSpan();
    bool hasDefinitePrimary = primaryStyle.IsDefinite();
    OriginZeroLine primaryGridStart = matrix.Counts(primaryAxis)
                                          .ImplicitStartLine();
    OriginZeroLine primaryGridEnd = matrix.Counts(primaryAxis)
                                        .ImplicitEndLine();
    OriginZeroLine secondaryGridStart = matrix.Counts(secondaryAxis)
                                            .ImplicitStartLine();
    OriginZeroLine secondaryGridEnd = matrix.Counts(secondaryAxis)
                                          .ImplicitEndLine();
    OriginZeroLine primaryStartPosition =
        SearchStartLine(primaryGridStart, primaryGridEnd, primaryReversed);
    OriginZeroLine secondaryStartPosition = SearchStartLine(
        secondaryGridStart, secondaryGridEnd, secondaryReversed);

    OriginZeroLine primaryIdx = gridPrimary;
    OriginZeroLine secondaryIdx = gridSecondary;

    if (hasDefinitePrimary) {
        LineOzl primarySpan =
            MaybeMirrorSpan(primaryStyle.ResolveDefiniteGridLines(),
                            primaryAxis, direction, explicitColCount);
        if (IsDense(autoFlow)) {
            secondaryIdx = secondaryStartPosition;
        } else {
            bool shouldAdvance = primaryReversed
                                     ? primarySpan.start > primaryIdx
                                     : primarySpan.start < primaryIdx;
            if (shouldAdvance) {
                secondaryIdx = AdvancePosition(secondaryIdx, secondaryReversed);
            }
        }
        // A fixed primary position: step the secondary axis until it fits.
        while (true) {
            LineOzl secondarySpan = ResolveIndefiniteGridSpan(
                secondaryIdx, secondarySpanLen, secondaryReversed);
            if (matrix.LineAreaIsUnoccupied(primaryAxis, primarySpan,
                                            secondarySpan)) {
                return {primarySpan, secondarySpan};
            }
            secondaryIdx = AdvancePosition(secondaryIdx, secondaryReversed);
        }
    }

    uint16_t primarySpanLen = primaryStyle.IndefiniteSpan();
    // Neither axis is fixed: walk the primary axis until the end of the
    // existing tracks, then reset it and step the secondary axis.
    while (true) {
        LineOzl primarySpan = ResolveIndefiniteGridSpan(
            primaryIdx, primarySpanLen, primaryReversed);
        LineOzl secondarySpan = ResolveIndefiniteGridSpan(
            secondaryIdx, secondarySpanLen, secondaryReversed);

        bool primaryOutOfBounds = primaryReversed
                                      ? primarySpan.start < primaryGridStart
                                      : primarySpan.end > primaryGridEnd;
        if (primaryOutOfBounds) {
            secondaryIdx = AdvancePosition(secondaryIdx, secondaryReversed);
            primaryIdx = primaryStartPosition;
            continue;
        }
        if (!matrix.LineAreaIsUnoccupied(primaryAxis, primarySpan,
                                         secondarySpan)) {
            primaryIdx = AdvancePosition(primaryIdx, primaryReversed);
            continue;
        }
        return {primarySpan, secondarySpan};
    }
}

// 8.5 Grid Item Placement Algorithm
// https://www.w3.org/TR/css-grid-2/#auto-placement-algo
void PlaceGridItems(CellOccupancyMatrix* matrix, Vec<GridItem>* items,
                    TaffyTree* tree, const Vec<PlacementChild>& children,
                    Direction direction, GridAutoFlow gridAutoFlow,
                    AlignItems alignItems, AlignItems justifyItems) {
    AbsoluteAxis primaryAxis = PrimaryAxis(gridAutoFlow);
    AbsoluteAxis secondaryAxis = OtherAxis(primaryAxis);
    uint16_t explicitColCount = matrix->Counts(AbsoluteAxis::Horizontal)
                                    .explicitCount;

    // 1. Children with definite positions in both axes.
    for (int i = 0; i < children.len; i++) {
        const PlacementChild& c = children[i];
        if (!c.horizontal.IsDefinite() || !c.vertical.IsDefinite()) {
            continue;
        }
        LineOzl primarySpan = PlaceDefiniteGridItemAxis(
            c, primaryAxis, direction, explicitColCount);
        LineOzl secondarySpan = PlaceDefiniteGridItemAxis(
            c, secondaryAxis, direction, explicitColCount);
        RecordGridPlacement(matrix, items, tree, c.node, c.index, alignItems,
                            justifyItems, primaryAxis, primarySpan,
                            secondarySpan,
                            CellOccupancyState::DefinitelyPlaced);
    }

    // 2. Children with a definite secondary axis position only.
    for (int i = 0; i < children.len; i++) {
        const PlacementChild& c = children[i];
        if (!c.Get(secondaryAxis).IsDefinite() || c.Get(primaryAxis)
                                                      .IsDefinite()) {
            continue;
        }
        SpanPair spans = PlaceDefiniteSecondaryAxisItem(
            *matrix, c, gridAutoFlow, direction, explicitColCount);
        RecordGridPlacement(matrix, items, tree, c.node, c.index, alignItems,
                            justifyItems, primaryAxis, spans.primary,
                            spans.secondary, CellOccupancyState::AutoPlaced);
    }

    // 3. Determining the number of columns in the implicit grid is already
    //    covered: the size estimate pre-sizes the matrix, and MarkAreaAs
    //    expands it as needed.

    // 4. The remaining items.
    bool primaryReversed = AxisIsReversed(direction, primaryAxis);
    OriginZeroLine startPrimary = SearchStartLine(
        matrix->Counts(primaryAxis).ImplicitStartLine(),
        matrix->Counts(primaryAxis).ImplicitEndLine(), primaryReversed);
    OriginZeroLine startSecondary =
        SearchStartLine(matrix->Counts(secondaryAxis).ImplicitStartLine(),
                        matrix->Counts(secondaryAxis).ImplicitEndLine(),
                        AxisIsReversed(direction, secondaryAxis));
    OriginZeroLine posPrimary = startPrimary;
    OriginZeroLine posSecondary = startSecondary;

    for (int i = 0; i < children.len; i++) {
        const PlacementChild& c = children[i];
        if (c.Get(secondaryAxis).IsDefinite()) {
            continue;
        }
        SpanPair spans = PlaceIndefinitelyPositionedItem(
            *matrix, c, gridAutoFlow, posPrimary, posSecondary, direction,
            explicitColCount);
        RecordGridPlacement(matrix, items, tree, c.node, c.index, alignItems,
                            justifyItems, primaryAxis, spans.primary,
                            spans.secondary, CellOccupancyState::AutoPlaced);

        // Dense packing restarts from the beginning for the next item; sparse
        // packing carries on from this one.
        if (IsDense(gridAutoFlow)) {
            posPrimary = startPrimary;
            posSecondary = startSecondary;
        } else {
            posPrimary =
                primaryReversed ? spans.primary.start : spans.primary.end;
            posSecondary = spans.secondary.start;
        }
    }
}

// ─── track_sizing.rs ─────────────────────────────────────────────────────

// Whether a minimum or a maximum size's space is being distributed. Controls
// what happens when distributing beyond limits.
// https://www.w3.org/TR/css-grid-1/#extra-space
enum class IntrinsicContributionType : uint8_t {
    Minimum,
    Maximum
};

// Walks a list of items sorted by (crosses a flex track, span) in batches of
// equal span, stopping once it reaches the flex-crossing items, which form one
// final batch.
struct ItemBatcher {
    AbstractAxis axis;
    int indexOffset = 0;
    uint16_t currentSpan = 1;
    bool currentIsFlex = false;

    bool Next(GridItem* items, int n, int* outStart, int* outEnd,
              bool* outIsFlex) {
        if (currentIsFlex || indexOffset >= n) {
            return false;
        }
        const GridItem& item = items[indexOffset];
        currentSpan = item.Span(axis);
        currentIsFlex = item.CrossesFlexibleTrack(axis);

        int nextIndexOffset = n;
        if (!currentIsFlex) {
            for (int i = 0; i < n; i++) {
                if (items[i].CrossesFlexibleTrack(axis) ||
                    items[i].Span(axis) > currentSpan) {
                    nextIndexOffset = i;
                    break;
                }
            }
        }
        *outStart = indexOffset;
        *outEnd = nextIndexOffset;
        *outIsFlex = currentIsFlex;
        indexOffset = nextIndexOffset;
        return true;
    }
};

// The variables the intrinsic size computations need, kept together so they do
// not have to be threaded through every call.
struct IntrinsicSizeMeasurer {
    TaffyTree* tree;
    const GridTrack* otherAxisTracks;
    TrackSizeEstimate estimate;
    AbstractAxis axis;
    SizeOptF innerNodeSize;

    CalcResolver Calc() const { return tree->calc; }

    SizeOptF GridAreaSize(GridItem* item, const GridTrack* axisTracks) const {
        return ItemGridAreaSizeCached(item, axis, axisTracks, otherAxisTracks,
                                      innerNodeSize, estimate, tree->calc);
    }
    float MinContentContribution(GridItem* item,
                                 const GridTrack* axisTracks) const {
        SizeOptF gridAreaSize = GridAreaSize(item, axisTracks);
        SizeOptF availableSpace = gridAreaSize;
        availableSpace.Set(axis, Optf());
        SizeF marginAxisSums = MarginsAxisSumsWithBaselineShims(
            *item, availableSpace.width, tree->calc);
        float contribution = ItemMinContentContributionCached(
            item, axis, tree, gridAreaSize, availableSpace);
        return contribution + marginAxisSums.Get(axis);
    }
    float MaxContentContribution(GridItem* item,
                                 const GridTrack* axisTracks) const {
        SizeOptF gridAreaSize = GridAreaSize(item, axisTracks);
        SizeOptF availableSpace = gridAreaSize;
        availableSpace.Set(axis, Optf());
        SizeF marginAxisSums = MarginsAxisSumsWithBaselineShims(
            *item, availableSpace.width, tree->calc);
        float contribution = ItemMaxContentContributionCached(
            item, axis, tree, gridAreaSize, availableSpace);
        return contribution + marginAxisSums.Get(axis);
    }
    float MinimumContribution(GridItem* item, const GridTrack* axisTracks,
                              int axisTrackCount) const {
        SizeOptF gridAreaSize = GridAreaSize(item, axisTracks);
        SizeOptF availableSpace = gridAreaSize;
        availableSpace.Set(axis, Optf());
        SizeF marginAxisSums = MarginsAxisSumsWithBaselineShims(
            *item, availableSpace.width, tree->calc);
        float contribution = ItemMinimumContributionCached(
            item, tree, axis, axisTracks, axisTrackCount, gridAreaSize,
            innerNodeSize);
        return contribution + marginAxisSums.Get(axis);
    }
};

// Track sizing wants the items in ascending order of span, with the items that
// cross a flexible track last.
bool CmpByCrossFlexThenSpanThenStart(const GridItem& a, const GridItem& b,
                                     AbstractAxis axis) {
    bool af = a.CrossesFlexibleTrack(axis);
    bool bf = b.CrossesFlexibleTrack(axis);
    if (af != bf) {
        return !af;
    }
    LineOzl pa = a.Placement(axis);
    LineOzl pb = b.Placement(axis);
    if (pa.Span() != pb.Span()) {
        return pa.Span() < pb.Span();
    }
    return pa.start < pb.start;
}

// When estimating the other-axis size of content-sized items, align-content
// and justify-content should be taken into account if the container and all
// the items in that axis have definite sizes. This is the per-gutter
// adjustment that comes out of that.
float ComputeAlignmentGutterAdjustment(AlignContent alignment,
                                       Optf axisInnerNodeSize,
                                       const GridTrack* tracks, int nTracks,
                                       TrackSizeEstimate estimate,
                                       CalcResolver calc) {
    if (nTracks <= 1) {
        return 0.0f;
    }
    // Items never cross the outermost gutters, so Start and End can be treated
    // alike. The safety modifier does not change a gutter's weight; the
    // overflow fallback happens when offsets are computed.
    int outerGutterWeight = 0;
    int innerGutterWeight = 0;
    switch (alignment.Keyword()) {
        case AlignContentKeyword::Start:
        case AlignContentKeyword::FlexStart:
        case AlignContentKeyword::End:
        case AlignContentKeyword::FlexEnd:
        case AlignContentKeyword::Center:
            outerGutterWeight = 1;
            innerGutterWeight = 0;
            break;
        case AlignContentKeyword::Stretch:
            outerGutterWeight = 0;
            innerGutterWeight = 0;
            break;
        case AlignContentKeyword::SpaceBetween:
            outerGutterWeight = 0;
            innerGutterWeight = 1;
            break;
        case AlignContentKeyword::SpaceAround:
            outerGutterWeight = 1;
            innerGutterWeight = 2;
            break;
        case AlignContentKeyword::SpaceEvenly:
            outerGutterWeight = 1;
            innerGutterWeight = 1;
            break;
    }
    if (innerGutterWeight == 0) {
        return 0.0f;
    }
    if (!axisInnerNodeSize.IsSome()) {
        return 0.0f;
    }

    float trackSizeSum = 0.0f;
    bool definite = true;
    for (int i = 0; i < nTracks; i++) {
        Optf v =
            EstimateTrackSize(tracks[i], axisInnerNodeSize, estimate, calc);
        if (!v.IsSome()) {
            definite = false;
            break;
        }
        trackSizeSum += v.val;
    }
    float freeSpace =
        definite ? F32Max(0.0f, axisInnerNodeSize.val - trackSizeSum) : 0.0f;

    int weightedTrackCount =
        (((nTracks - 3) / 2) * innerGutterWeight) + (2 * outerGutterWeight);
    if (weightedTrackCount == 0) {
        return 0.0f;
    }
    return (freeSpace / (float)weightedTrackCount) * (float)innerGutterWeight;
}

void ResolveItemTrackIndexes(GridItem* items, int n, TrackCounts columnCounts,
                             TrackCounts rowCounts) {
    for (int i = 0; i < n; i++) {
        GridItem& item = items[i];
        item.columnIndexes.start =
            (uint16_t)IntoTrackVecIndex(item.column.start, columnCounts);
        item.columnIndexes
            .end = (uint16_t)IntoTrackVecIndex(item.column.end, columnCounts);
        item.rowIndexes
            .start = (uint16_t)IntoTrackVecIndex(item.row.start, rowCounts);
        item.rowIndexes
            .end = (uint16_t)IntoTrackVecIndex(item.row.end, rowCounts);
    }
}

void DetermineIfItemCrossesFlexibleOrIntrinsicTracks(GridItem* items, int n,
                                                     const GridTrack* columns,
                                                     const GridTrack* rows) {
    for (int i = 0; i < n; i++) {
        GridItem& item = items[i];
        item.crossesFlexibleColumn = false;
        item.crossesIntrinsicColumn = false;
        for (int k = item.TrackRangeStart(AbstractAxis::Inline);
             k < item.TrackRangeEnd(AbstractAxis::Inline); k++) {
            if (columns[k].IsFlexible()) {
                item.crossesFlexibleColumn = true;
            }
            if (columns[k].HasIntrinsicSizingFunction()) {
                item.crossesIntrinsicColumn = true;
            }
        }
        item.crossesFlexibleRow = false;
        item.crossesIntrinsicRow = false;
        for (int k = item.TrackRangeStart(AbstractAxis::Block);
             k < item.TrackRangeEnd(AbstractAxis::Block); k++) {
            if (rows[k].IsFlexible()) {
                item.crossesFlexibleRow = true;
            }
            if (rows[k].HasIntrinsicSizingFunction()) {
                item.crossesIntrinsicRow = true;
            }
        }
    }
}

void FlushPlannedBaseSizeIncreases(GridTrack* tracks, int n) {
    for (int i = 0; i < n; i++) {
        tracks[i].baseSize += tracks[i].baseSizePlannedIncrease;
        tracks[i].baseSizePlannedIncrease = 0.0f;
    }
}

void FlushPlannedGrowthLimitIncreases(GridTrack* tracks, int n,
                                      bool setInfinitelyGrowable) {
    for (int i = 0; i < n; i++) {
        GridTrack& t = tracks[i];
        if (t.growthLimitPlannedIncrease > 0.0f) {
            t.growthLimit = t.growthLimit == INFINITY
                                ? t.baseSize + t.growthLimitPlannedIncrease
                                : t.growthLimit + t.growthLimitPlannedIncrease;
            t.infinitelyGrowable = setInfinitelyGrowable;
        } else {
            t.infinitelyGrowable = false;
        }
        t.growthLimitPlannedIncrease = 0.0f;
    }
}

// 11.4 Initialise Track sizes
void InitializeTrackSizes(TaffyTree* tree, GridTrack* tracks, int n,
                          Optf axisInnerNodeSize) {
    CalcResolver calc = tree->calc;
    for (int i = 0; i < n; i++) {
        GridTrack& t = tracks[i];
        // A fixed min sizing function resolves to a length and becomes the
        // initial base size; an intrinsic one starts at zero.
        t.baseSize = t.minTrackSizingFunction
                         .DefiniteValue(axisInnerNodeSize, calc)
                         .UnwrapOr(0.0f);
        // A fixed max sizing function becomes the initial growth limit; an
        // intrinsic or flexible one starts at infinity.
        t.growthLimit = t.maxTrackSizingFunction
                            .DefiniteValue(axisInnerNodeSize, calc)
                            .UnwrapOr(INFINITY);
        if (t.growthLimit < t.baseSize) {
            t.growthLimit = t.baseSize;
        }
    }
}

// 11.5.1 Shim baseline-aligned items so their intrinsic size contributions
// reflect their baseline alignment.
void ResolveItemBaselines(TaffyTree* tree, AbstractAxis axis, GridItem* items,
                          int n, SizeOptF innerNodeSize) {
    AbstractAxis otherAxis = Other(axis);
    StableSort(items, n, [&](const GridItem& a, const GridItem& b) {
        return a.Placement(otherAxis).start < b.Placement(otherAxis).start;
    });

    int start = 0;
    while (start < n) {
        OriginZeroLine currentRow = items[start].Placement(otherAxis).start;
        int end = start;
        while (end < n && items[end].Placement(otherAxis).start == currentRow) {
            end++;
        }

        // One or zero participating items makes baseline alignment a no-op.
        int baselineCount = 0;
        for (int i = start; i < end; i++) {
            if (items[i].alignSelf.keyword == AlignItemsKeyword::Baseline) {
                baselineCount++;
            }
        }
        if (baselineCount <= 1) {
            start = end;
            continue;
        }

        for (int i = start; i < end; i++) {
            GridItem& item = items[i];
            LayoutOutput out = tree->PerformChildLayout(
                item.node, SizeOptF::None(), innerNodeSize,
                SizeAvail::MinContent(), SizingMode::InherentSize,
                LineBool::False());
            RectLpa topOnly = {LengthPercentageAuto::Zero(),
                               LengthPercentageAuto::Zero(), item.margin.top,
                               LengthPercentageAuto::Zero()};
            float marginTop =
                topOnly.ResolveOrZero(innerNodeSize.width, tree->calc).top;
            item.baseline = Optf(
                out.firstBaselines.y.UnwrapOr(out.size.height) + marginTop);
        }

        float rowMaxBaseline = 0.0f;
        for (int i = start; i < end; i++) {
            rowMaxBaseline =
                F32Max(rowMaxBaseline, items[i].baseline.UnwrapOr(0.0f));
        }
        for (int i = start; i < end; i++) {
            items[i].baselineShim =
                rowMaxBaseline - items[i].baseline.UnwrapOr(0.0f);
        }
        start = end;
    }
}

// Distribute space to tracks evenly, up to their limits. Used by both the
// base-size and the maximise-tracks steps.
template <typename Affected, typename Proportion, typename Property,
          typename Limit>
float DistributeSpaceUpToLimits(float spaceToDistribute, GridTrack* tracks,
                                int n, Affected trackIsAffected,
                                Proportion trackDistributionProportion,
                                Property trackAffectedProperty,
                                Limit trackLimit) {
    // A small constant so rounding error cannot spin this loop forever.
    const float kThreshold = 0.01f;

    while (spaceToDistribute > kThreshold) {
        float proportionSum = 0.0f;
        for (int i = 0; i < n; i++) {
            const GridTrack& t = tracks[i];
            if (trackAffectedProperty(t) + t.itemIncurredIncrease <
                    trackLimit(t) &&
                trackIsAffected(t)) {
                proportionSum += trackDistributionProportion(t);
            }
        }
        if (proportionSum == 0.0f) {
            break;
        }

        bool haveMin = false;
        float minIncreaseLimit = 0.0f;
        for (int i = 0; i < n; i++) {
            const GridTrack& t = tracks[i];
            if (trackAffectedProperty(t) + t.itemIncurredIncrease <
                    trackLimit(t) &&
                trackIsAffected(t)) {
                float v = (trackLimit(t) - trackAffectedProperty(t)) /
                          trackDistributionProportion(t);
                if (!haveMin || v < minIncreaseLimit) {
                    minIncreaseLimit = v;
                    haveMin = true;
                }
            }
        }
        if (!haveMin) {
            break;
        }
        float iterationIncrease =
            F32Min(minIncreaseLimit, spaceToDistribute / proportionSum);

        for (int i = 0; i < n; i++) {
            GridTrack& t = tracks[i];
            if (!trackIsAffected(t)) {
                continue;
            }
            float increase = iterationIncrease * trackDistributionProportion(t);
            if (increase > 0.0f && trackAffectedProperty(t) + increase <=
                                       trackLimit(t) + kThreshold) {
                t.itemIncurredIncrease += increase;
                spaceToDistribute -= increase;
            }
        }
    }
    return spaceToDistribute;
}

// 11.5.1 Distributing Extra Space Across Spanned Tracks
// https://www.w3.org/TR/css-grid-1/#extra-space
template <typename Affected, typename Proportion, typename Limit>
void DistributeItemSpaceToBaseSizeInner(
    float space, GridTrack* tracks, int n, Affected trackIsAffected,
    Proportion trackDistributionProportion, Limit trackLimit,
    IntrinsicContributionType intrinsicContributionType) {
    if (space == 0.0f) {
        return;
    }
    bool anyAffected = false;
    for (int i = 0; i < n; i++) {
        if (trackIsAffected(tracks[i])) {
            anyAffected = true;
            break;
        }
    }
    if (!anyAffected) {
        return;
    }

    auto getBaseSize = [](const GridTrack& t) { return t.baseSize; };

    // 1. Find the space to distribute.
    float trackSizes = 0.0f;
    for (int i = 0; i < n; i++) {
        trackSizes += tracks[i].baseSize;
    }
    float extraSpace = F32Max(0.0f, space - trackSizes);

    // 2. Distribute up to the limits.
    const float kThreshold = 0.000001f;
    extraSpace = DistributeSpaceUpToLimits(
        extraSpace, tracks, n, trackIsAffected, trackDistributionProportion,
        getBaseSize, trackLimit);

    // 3. Distribute what remains beyond the limits.
    if (extraSpace > kThreshold) {
        // Accommodating minimum or min-content contributions: any affected
        // track that also has an intrinsic max sizing function. Accommodating
        // max-content contributions: any that also has a max-content max
        // sizing function.
        auto minimumFilter = [](const GridTrack& t) {
            return t.maxTrackSizingFunction.IsIntrinsic();
        };
        auto maximumFilter = [](const GridTrack& t) {
            return t.minTrackSizingFunction.IsMaxContent() ||
                   t.maxTrackSizingFunction.IsMaxOrFitContent();
        };
        int count = 0;
        for (int i = 0; i < n; i++) {
            const GridTrack& t = tracks[i];
            bool matches =
                intrinsicContributionType == IntrinsicContributionType::Minimum
                    ? minimumFilter(t)
                    : maximumFilter(t);
            if (trackIsAffected(t) && matches) {
                count++;
            }
        }
        // With no such tracks, use all the affected ones.
        bool useAll = count == 0;
        auto filter = [&](const GridTrack& t) {
            if (useAll) {
                return true;
            }
            return intrinsicContributionType ==
                           IntrinsicContributionType::Minimum
                       ? minimumFilter(t)
                       : maximumFilter(t);
        };
        DistributeSpaceUpToLimits(extraSpace, tracks, n, filter,
                                  trackDistributionProportion, getBaseSize,
                                  trackLimit);
    }

    // 4. Roll each track's item-incurred increase into its planned increase.
    for (int i = 0; i < n; i++) {
        GridTrack& t = tracks[i];
        if (t.itemIncurredIncrease > t.baseSizePlannedIncrease) {
            t.baseSizePlannedIncrease = t.itemIncurredIncrease;
        }
        t.itemIncurredIncrease = 0.0f;
    }
}

template <typename Affected, typename Limit>
void DistributeItemSpaceToBaseSize(
    bool isFlex, bool useFlexFactorForDistribution, float space,
    GridTrack* tracks, int n, Affected trackIsAffected, Limit trackLimit,
    IntrinsicContributionType intrinsicContributionType) {
    auto one = [](const GridTrack&) { return 1.0f; };
    if (isFlex) {
        auto filter = [&](const GridTrack& t) {
            return t.IsFlexible() && trackIsAffected(t);
        };
        if (useFlexFactorForDistribution) {
            auto flexFactor = [](const GridTrack& t) { return t.FlexFactor(); };
            DistributeItemSpaceToBaseSizeInner(space, tracks, n, filter,
                                               flexFactor, trackLimit,
                                               intrinsicContributionType);
        } else {
            DistributeItemSpaceToBaseSizeInner(space, tracks, n, filter, one,
                                               trackLimit,
                                               intrinsicContributionType);
        }
        return;
    }
    DistributeItemSpaceToBaseSizeInner(space, tracks, n, trackIsAffected, one,
                                       trackLimit, intrinsicContributionType);
}

// The same, simplified, for growth limits.
template <typename Affected>
void DistributeItemSpaceToGrowthLimit(float space, GridTrack* tracks, int n,
                                      Affected trackIsAffected,
                                      Optf axisInnerNodeSize) {
    if (space == 0.0f) {
        return;
    }
    int affected = 0;
    for (int i = 0; i < n; i++) {
        if (trackIsAffected(tracks[i])) {
            affected++;
        }
    }
    if (affected == 0) {
        return;
    }

    // 1. Find the space to distribute.
    float trackSizes = 0.0f;
    for (int i = 0; i < n; i++) {
        trackSizes += tracks[i].growthLimit == INFINITY ? tracks[i].baseSize
                                                        : tracks[i].growthLimit;
    }
    float extraSpace = F32Max(0.0f, space - trackSizes);

    // 2. Distribute up to the limits. For growth limits the limit is either
    //    infinity or the growth limit itself, so either everything goes to the
    //    infinite tracks or nothing is distributed here.
    auto growable = [&](const GridTrack& t) {
        return trackIsAffected(t) &&
               (t.infinitelyGrowable ||
                t.FitContentLimitedGrowthLimit(axisInnerNodeSize) == INFINITY);
    };
    int growableCount = 0;
    for (int i = 0; i < n; i++) {
        if (growable(tracks[i])) {
            growableCount++;
        }
    }
    if (growableCount > 0) {
        float increase = extraSpace / (float)growableCount;
        for (int i = 0; i < n; i++) {
            if (growable(tracks[i])) {
                tracks[i].itemIncurredIncrease = increase;
            }
        }
    } else {
        // 3. Distribute beyond the limits, to all affected tracks.
        DistributeSpaceUpToLimits(
            extraSpace, tracks, n, trackIsAffected,
            [](const GridTrack&) { return 1.0f; },
            [](const GridTrack& t) {
                return t.growthLimit == INFINITY ? t.baseSize : t.growthLimit;
            },
            [&](const GridTrack& t) {
                return t.FitContentLimit(axisInnerNodeSize);
            });
    }

    // 4. Roll each track's item-incurred increase into its planned increase.
    for (int i = 0; i < n; i++) {
        GridTrack& t = tracks[i];
        if (t.itemIncurredIncrease > t.growthLimitPlannedIncrease) {
            t.growthLimitPlannedIncrease = t.itemIncurredIncrease;
        }
        t.itemIncurredIncrease = 0.0f;
    }
}

// 11.5 Resolve Intrinsic Track Sizes
void ResolveIntrinsicTrackSizes(TaffyTree* tree, AbstractAxis axis,
                                GridTrack* axisTracks, int nAxisTracks,
                                const GridTrack* otherAxisTracks,
                                GridItem* items, int nItems,
                                AvailableSpace axisAvailableGridSpace,
                                SizeOptF innerNodeSize,
                                TrackSizeEstimate estimate) {
    // Step 1 (shimming baseline-aligned items) is already done.

    // The algorithm walks items in ascending order of the number of tracks
    // they span, so they are pre-sorted into that order here.
    StableSort(items, nItems, [&](const GridItem& a, const GridItem& b) {
        return CmpByCrossFlexThenSpanThenStart(a, b, axis);
    });

    Optf axisInnerNodeSize = innerNodeSize.Get(axis);
    float flexFactorSum = 0.0f;
    for (int i = 0; i < nAxisTracks; i++) {
        flexFactorSum += axisTracks[i].FlexFactor();
    }
    IntrinsicSizeMeasurer sizer = {tree, otherAxisTracks, estimate, axis,
                                   innerNodeSize};
    CalcResolver calc = tree->calc;

    ItemBatcher batcher;
    batcher.axis = axis;
    int batchStart = 0;
    int batchEnd = 0;
    bool isFlex = false;
    while (batcher.Next(items, nItems, &batchStart, &batchEnd, &isFlex)) {
        GridItem* batch = items + batchStart;
        int batchLen = batchEnd - batchStart;
        uint16_t batchSpan = batch[0].Placement(axis).Span();

        // 2. Size tracks to fit non-spanning items.
        if (!isFlex && batchSpan == 1) {
            for (int bi = 0; bi < batchLen; bi++) {
                GridItem* item = &batch[bi];
                int trackIndex = (int)item->PlacementIndexes(axis).start + 1;
                const GridTrack& track = axisTracks[trackIndex];
                Overflow axisOverflow = axis == AbstractAxis::Inline
                                            ? item->overflow.x
                                            : item->overflow.y;

                float newBaseSize = track.baseSize;
                switch (track.minTrackSizingFunction.raw.Tag()) {
                    case CompactLength::kMinContentTag:
                        newBaseSize = F32Max(
                            track.baseSize,
                            sizer.MinContentContribution(item, axisTracks));
                        break;
                    case CompactLength::kPercentTag:
                        // An indefinite, not-yet-resolved container size makes
                        // a percentage track behave as min-content, matching
                        // Chrome.
                        if (!axisInnerNodeSize.IsSome()) {
                            newBaseSize = F32Max(
                                track.baseSize,
                                sizer.MinContentContribution(item, axisTracks));
                        }
                        break;
                    case CompactLength::kMaxContentTag:
                        newBaseSize = F32Max(
                            track.baseSize,
                            sizer.MaxContentContribution(item, axisTracks));
                        break;
                    case CompactLength::kAutoTag: {
                        float space;
                        bool minOrMaxConstraint =
                            axisAvailableGridSpace
                                    .kind == AvailableSpace::Kind::MinContent ||
                            axisAvailableGridSpace
                                    .kind == AvailableSpace::Kind::MaxContent;
                        // QUIRK: the spec says a container sized under a min-
                        // or max-content constraint uses the items' limited
                        // min-content contributions in place of their minimum
                        // contributions. Browsers only do that when the item
                        // is not a scroll container, giving a scroll
                        // container's automatic minimum size (zero)
                        // precedence.
                        if (minOrMaxConstraint &&
                            !IsScrollContainer(axisOverflow)) {
                            float axisMinimumSize = sizer.MinimumContribution(
                                item, axisTracks, nAxisTracks);
                            float axisMinContentSize =
                                sizer.MinContentContribution(item, axisTracks);
                            Optf limit =
                                track.maxTrackSizingFunction
                                    .DefiniteLimit(axisInnerNodeSize, calc);
                            space = F32Max(MaybeMin(axisMinContentSize, limit),
                                           axisMinimumSize);
                        } else {
                            space = sizer.MinimumContribution(item, axisTracks,
                                                              nAxisTracks);
                        }
                        newBaseSize = F32Max(track.baseSize, space);
                        break;
                    }
                    case CompactLength::kLengthTag:
                        // Not an intrinsic track sizing function.
                        break;
                    default:
                        // calc() behaves like a percentage here.
                        if (track.minTrackSizingFunction.raw.IsCalc() &&
                            !axisInnerNodeSize.IsSome()) {
                            newBaseSize = F32Max(
                                track.baseSize,
                                sizer.MinContentContribution(item, axisTracks));
                        }
                        break;
                }

                bool haveGrowthLimitMinContent =
                    !IsScrollContainer(axisOverflow);
                float growthLimitMinContent =
                    haveGrowthLimitMinContent
                        ? sizer.MinContentContribution(item, axisTracks)
                        : 0.0f;
                float growthLimitMaxContent =
                    sizer.MaxContentContribution(item, axisTracks);
                float growthLimitIntrinsicMinContent =
                    sizer.MinContentContribution(item, axisTracks);

                GridTrack& t = axisTracks[trackIndex];
                t.baseSize = newBaseSize;

                if (t.maxTrackSizingFunction.IsFitContent()) {
                    // An item that is not a scroll container raises the growth
                    // limit to at least its min-content contribution.
                    if (haveGrowthLimitMinContent) {
                        t.growthLimitPlannedIncrease =
                            F32Max(t.growthLimitPlannedIncrease,
                                   growthLimitMinContent);
                    }
                    // And always to at least the fit-content-limited
                    // max-content contribution.
                    float fitContentLimit =
                        t.FitContentLimit(axisInnerNodeSize);
                    float maxContentContribution =
                        F32Min(growthLimitMaxContent, fitContentLimit);
                    t.growthLimitPlannedIncrease = F32Max(
                        t.growthLimitPlannedIncrease, maxContentContribution);
                } else if (t.maxTrackSizingFunction.IsMaxContentAlike() ||
                           (t.maxTrackSizingFunction.UsesPercentage() &&
                            !axisInnerNodeSize.IsSome())) {
                    // An indefinite, not-yet-resolved container size makes a
                    // percentage track behave as auto, matching Chrome.
                    t.growthLimitPlannedIncrease = F32Max(
                        t.growthLimitPlannedIncrease, growthLimitMaxContent);
                } else if (t.maxTrackSizingFunction.IsIntrinsic()) {
                    t.growthLimitPlannedIncrease =
                        F32Max(t.growthLimitPlannedIncrease,
                               growthLimitIntrinsicMinContent);
                }
            }

            for (int i = 0; i < nAxisTracks; i++) {
                GridTrack& t = axisTracks[i];
                if (t.growthLimitPlannedIncrease > 0.0f) {
                    t.growthLimit = t.growthLimit == INFINITY
                                        ? t.growthLimitPlannedIncrease
                                        : F32Max(t.growthLimit,
                                                 t.growthLimitPlannedIncrease);
                }
                t.infinitelyGrowable = false;
                t.growthLimitPlannedIncrease = 0.0f;
                if (t.growthLimit < t.baseSize) {
                    t.growthLimit = t.baseSize;
                }
            }
            continue;
        }

        bool useFlexFactorForDistribution = isFlex && flexFactorSum != 0.0f;

        // 1. For intrinsic minimums.
        for (int bi = 0; bi < batchLen; bi++) {
            GridItem* item = &batch[bi];
            if (!item->CrossesIntrinsicTrack(axis)) {
                continue;
            }
            Overflow axisOverflow = axis == AbstractAxis::Inline
                                        ? item->overflow.x
                                        : item->overflow.y;
            bool minOrMaxConstraint =
                axisAvailableGridSpace
                        .kind == AvailableSpace::Kind::MinContent ||
                axisAvailableGridSpace.kind == AvailableSpace::Kind::MaxContent;
            float space;
            if (minOrMaxConstraint && !IsScrollContainer(axisOverflow)) {
                float axisMinimumSize =
                    sizer.MinimumContribution(item, axisTracks, nAxisTracks);
                float axisMinContentSize =
                    sizer.MinContentContribution(item, axisTracks);
                Optf limit = SpannedTrackLimit(*item, axis, axisTracks,
                                               axisInnerNodeSize, calc);
                space = F32Max(MaybeMin(axisMinContentSize, limit),
                               axisMinimumSize);
            } else {
                space = sizer
                            .MinimumContribution(item, axisTracks, nAxisTracks);
            }
            int from = item->TrackRangeStart(axis);
            int count = item->TrackRangeEnd(axis) - from;
            if (space > 0.0f && count > 0) {
                auto hasIntrinsicMin = [&](const GridTrack& t) {
                    return !t.minTrackSizingFunction
                                .DefiniteValue(axisInnerNodeSize, calc)
                                .IsSome();
                };
                if (IsScrollContainer(axisOverflow)) {
                    DistributeItemSpaceToBaseSize(
                        isFlex, useFlexFactorForDistribution, space,
                        axisTracks + from, count, hasIntrinsicMin,
                        [&](const GridTrack& t) {
                            return t.FitContentLimitedGrowthLimit(
                                axisInnerNodeSize);
                        },
                        IntrinsicContributionType::Minimum);
                } else {
                    DistributeItemSpaceToBaseSize(
                        isFlex, useFlexFactorForDistribution, space,
                        axisTracks + from, count, hasIntrinsicMin,
                        [](const GridTrack& t) { return t.growthLimit; },
                        IntrinsicContributionType::Minimum);
                }
            }
        }
        FlushPlannedBaseSizeIncreases(axisTracks, nAxisTracks);

        // 2. For content-based minimums.
        auto hasMinOrMaxContentMin = [](const GridTrack& t) {
            return t.minTrackSizingFunction.IsMinOrMaxContent();
        };
        for (int bi = 0; bi < batchLen; bi++) {
            GridItem* item = &batch[bi];
            Overflow axisOverflow = axis == AbstractAxis::Inline
                                        ? item->overflow.x
                                        : item->overflow.y;
            float space = sizer.MinContentContribution(item, axisTracks);
            int from = item->TrackRangeStart(axis);
            int count = item->TrackRangeEnd(axis) - from;
            if (space > 0.0f && count > 0) {
                if (IsScrollContainer(axisOverflow)) {
                    DistributeItemSpaceToBaseSize(
                        isFlex, useFlexFactorForDistribution, space,
                        axisTracks + from, count, hasMinOrMaxContentMin,
                        [&](const GridTrack& t) {
                            return t.FitContentLimitedGrowthLimit(
                                axisInnerNodeSize);
                        },
                        IntrinsicContributionType::Minimum);
                } else {
                    DistributeItemSpaceToBaseSize(
                        isFlex, useFlexFactorForDistribution, space,
                        axisTracks + from, count, hasMinOrMaxContentMin,
                        [](const GridTrack& t) { return t.growthLimit; },
                        IntrinsicContributionType::Minimum);
                }
            }
        }
        FlushPlannedBaseSizeIncreases(axisTracks, nAxisTracks);

        // 3. For max-content minimums, under a max-content constraint.
        if (axisAvailableGridSpace.kind == AvailableSpace::Kind::MaxContent) {
            // A track with an auto min sizing function, excluding those whose
            // max is min-content. The exclusion matches Chrome, and follows
            // from minmax()'s "if the max is less than the min, the max is
            // floored by the min".
            auto hasAutoMin = [](const GridTrack& t) {
                return t.minTrackSizingFunction.IsAuto() &&
                       !t.maxTrackSizingFunction.IsMinContent();
            };
            auto hasMaxContentMin = [](const GridTrack& t) {
                return t.minTrackSizingFunction.IsMaxContent();
            };
            for (int bi = 0; bi < batchLen; bi++) {
                GridItem* item = &batch[bi];
                float axisMaxContentSize =
                    sizer.MaxContentContribution(item, axisTracks);
                Optf limit = SpannedTrackLimit(*item, axis, axisTracks,
                                               axisInnerNodeSize, calc);
                float space = MaybeMin(axisMaxContentSize, limit);
                int from = item->TrackRangeStart(axis);
                int count = item->TrackRangeEnd(axis) - from;
                if (space <= 0.0f || count <= 0) {
                    continue;
                }
                bool anyMaxContentMin = false;
                for (int k = from; k < from + count; k++) {
                    if (hasMaxContentMin(axisTracks[k])) {
                        anyMaxContentMin = true;
                        break;
                    }
                }
                // Distributing to MaxContent tracks in preference to Auto ones
                // is not in the spec, but both Chrome and Firefox do it.
                // https://www.w3.org/TR/css-grid-1/#track-size-max-content-min
                if (anyMaxContentMin) {
                    DistributeItemSpaceToBaseSize(
                        isFlex, useFlexFactorForDistribution, space,
                        axisTracks + from, count, hasMaxContentMin,
                        [](const GridTrack&) { return INFINITY; },
                        IntrinsicContributionType::Maximum);
                } else {
                    DistributeItemSpaceToBaseSize(
                        isFlex, useFlexFactorForDistribution, space,
                        axisTracks + from, count, hasAutoMin,
                        [&](const GridTrack& t) {
                            return t.FitContentLimitedGrowthLimit(
                                axisInnerNodeSize);
                        },
                        IntrinsicContributionType::Maximum);
                }
            }
            FlushPlannedBaseSizeIncreases(axisTracks, nAxisTracks);
        }

        // In all cases, keep growing tracks with a max-content min sizing
        // function to fit these items' max-content contributions.
        auto hasMaxContentMinFn = [](const GridTrack& t) {
            return t.minTrackSizingFunction.IsMaxContent();
        };
        for (int bi = 0; bi < batchLen; bi++) {
            GridItem* item = &batch[bi];
            float space = sizer.MaxContentContribution(item, axisTracks);
            int from = item->TrackRangeStart(axis);
            int count = item->TrackRangeEnd(axis) - from;
            if (space > 0.0f && count > 0) {
                DistributeItemSpaceToBaseSize(
                    isFlex, useFlexFactorForDistribution, space,
                    axisTracks + from, count, hasMaxContentMinFn,
                    [](const GridTrack& t) { return t.growthLimit; },
                    IntrinsicContributionType::Maximum);
            }
        }
        FlushPlannedBaseSizeIncreases(axisTracks, nAxisTracks);

        // 4. Any growth limit now below its base size rises to match it.
        for (int i = 0; i < nAxisTracks; i++) {
            if (axisTracks[i].growthLimit < axisTracks[i].baseSize) {
                axisTracks[i].growthLimit = axisTracks[i].baseSize;
            }
        }

        // A flexible track cannot also have an intrinsic max sizing function,
        // so steps 5 and 6 do not apply to one.
        if (!isFlex) {
            // 5. For intrinsic maximums.
            auto hasIntrinsicMax = [&](const GridTrack& t) {
                return !t.maxTrackSizingFunction
                            .HasDefiniteValue(axisInnerNodeSize);
            };
            for (int bi = 0; bi < batchLen; bi++) {
                GridItem* item = &batch[bi];
                float space = sizer.MinContentContribution(item, axisTracks);
                int from = item->TrackRangeStart(axis);
                int count = item->TrackRangeEnd(axis) - from;
                if (space > 0.0f && count > 0) {
                    DistributeItemSpaceToGrowthLimit(space, axisTracks + from,
                                                     count, hasIntrinsicMax,
                                                     axisInnerNodeSize);
                }
            }
            // Tracks whose growth limit went from infinite to finite here are
            // marked infinitely growable for the next step.
            FlushPlannedGrowthLimitIncreases(axisTracks, nAxisTracks, true);

            // 6. For max-content maximums, limiting fit-content() tracks by
            //    their argument.
            auto hasMaxContentMax = [&](const GridTrack& t) {
                return t.maxTrackSizingFunction.IsMaxContentAlike() ||
                       (t.maxTrackSizingFunction.UsesPercentage() &&
                        !axisInnerNodeSize.IsSome());
            };
            for (int bi = 0; bi < batchLen; bi++) {
                GridItem* item = &batch[bi];
                float space = sizer.MaxContentContribution(item, axisTracks);
                int from = item->TrackRangeStart(axis);
                int count = item->TrackRangeEnd(axis) - from;
                if (space > 0.0f && count > 0) {
                    DistributeItemSpaceToGrowthLimit(space, axisTracks + from,
                                                     count, hasMaxContentMax,
                                                     axisInnerNodeSize);
                }
            }
            FlushPlannedGrowthLimitIncreases(axisTracks, nAxisTracks, false);
        }
    }

    // Step 5. Any track still with an infinite growth limit — because nothing
    // was placed in it, or because it is flexible — takes its base size.
    // Important: without this the maximise-tracks step would affect flexible
    // tracks.
    for (int i = 0; i < nAxisTracks; i++) {
        if (axisTracks[i].growthLimit == INFINITY) {
            axisTracks[i].growthLimit = axisTracks[i].baseSize;
        }
    }
}

// 11.6 Maximise Tracks: distribute free space to tracks with finite growth
// limits, up to those limits.
void MaximiseTracks(GridTrack* axisTracks, int n, Optf axisInnerNodeSize,
                    AvailableSpace axisAvailableGridSpace) {
    float usedSpace = 0.0f;
    for (int i = 0; i < n; i++) {
        usedSpace += axisTracks[i].baseSize;
    }
    float freeSpace = axisAvailableGridSpace.ComputeFreeSpace(usedSpace);
    if (freeSpace == INFINITY) {
        for (int i = 0; i < n; i++) {
            axisTracks[i].baseSize = axisTracks[i].growthLimit;
        }
    } else if (freeSpace > 0.0f) {
        DistributeSpaceUpToLimits(
            freeSpace, axisTracks, n, [](const GridTrack&) { return true; },
            [](const GridTrack&) { return 1.0f; },
            [](const GridTrack& t) { return t.baseSize; },
            [&](const GridTrack& t) {
                return t.FitContentLimitedGrowthLimit(axisInnerNodeSize);
            });
        for (int i = 0; i < n; i++) {
            axisTracks[i].baseSize += axisTracks[i].itemIncurredIncrease;
            axisTracks[i].itemIncurredIncrease = 0.0f;
        }
    }
}

// 11.7.1 Find the Size of an fr: the largest an fr can be without exceeding
// the target size.
float FindSizeOfFr(const GridTrack* tracks, int n, float spaceToFill) {
    // The trivial case, which also keeps the loop below from spinning.
    if (spaceToFill == 0.0f) {
        return 0.0f;
    }

    // A flexible track whose flex factor times the hypothetical fr size is
    // below its base size has to be treated as inflexible, and the algorithm
    // restarted. Starting at infinity makes that impossible on the first pass.
    float hypotheticalFrSize = INFINITY;
    float previousIterHypotheticalFrSize;
    while (true) {
        float usedSpace = 0.0f;
        float naiveFlexFactorSum = 0.0f;
        for (int i = 0; i < n; i++) {
            const GridTrack& t = tracks[i];
            if (t.maxTrackSizingFunction.IsFr() &&
                t.maxTrackSizingFunction.raw.Value() * hypotheticalFrSize >=
                    t.baseSize) {
                naiveFlexFactorSum += t.maxTrackSizingFunction.raw.Value();
            } else {
                usedSpace += t.baseSize;
            }
        }
        float leftoverSpace = spaceToFill - usedSpace;
        float flexFactor = F32Max(naiveFlexFactorSum, 1.0f);

        previousIterHypotheticalFrSize = hypotheticalFrSize;
        hypotheticalFrSize = leftoverSpace / flexFactor;

        bool valid = true;
        for (int i = 0; i < n; i++) {
            const GridTrack& t = tracks[i];
            if (!t.maxTrackSizingFunction.IsFr()) {
                continue;
            }
            float ff = t.maxTrackSizingFunction.raw.Value();
            if (!(ff * hypotheticalFrSize >= t.baseSize ||
                  ff * previousIterHypotheticalFrSize < t.baseSize)) {
                valid = false;
                break;
            }
        }
        if (valid) {
            break;
        }
    }
    return hypotheticalFrSize;
}

// 11.7 Expand Flexible Tracks
void ExpandFlexibleTracks(TaffyTree* tree, AbstractAxis axis,
                          GridTrack* axisTracks, int nAxisTracks,
                          GridItem* items, int nItems, Optf axisMinSize,
                          Optf axisMaxSize,
                          AvailableSpace axisAvailableSpaceForExpansion) {
    float flexFraction = 0.0f;
    if (axisAvailableSpaceForExpansion.kind == AvailableSpace::Kind::Definite) {
        float availableSpace = axisAvailableSpaceForExpansion.value;
        float usedSpace = 0.0f;
        for (int i = 0; i < nAxisTracks; i++) {
            usedSpace += axisTracks[i].baseSize;
        }
        float freeSpace = availableSpace - usedSpace;
        flexFraction = freeSpace <= 0.0f ? 0.0f
                                         : FindSizeOfFr(axisTracks, nAxisTracks,
                                                        availableSpace);
    } else if (axisAvailableSpaceForExpansion
                   .kind == AvailableSpace::Kind::MinContent) {
        // Under a min-content constraint the used flex fraction is zero.
        flexFraction = 0.0f;
    } else {
        // Indefinite free space: the maximum of the per-track fractions and
        // the per-item ones.
        float trackMax = 0.0f;
        for (int i = 0; i < nAxisTracks; i++) {
            const GridTrack& t = axisTracks[i];
            if (!t.maxTrackSizingFunction.IsFr()) {
                continue;
            }
            float ff = t.FlexFactor();
            float v = ff > 1.0f ? t.baseSize / ff : t.baseSize;
            trackMax = F32Max(trackMax, v);
        }
        float itemMax = 0.0f;
        for (int i = 0; i < nItems; i++) {
            GridItem* item = &items[i];
            if (!item->CrossesFlexibleTrack(axis)) {
                continue;
            }
            int from = item->TrackRangeStart(axis);
            int count = item->TrackRangeEnd(axis) - from;
            // TODO(taffy): plumb an estimate of the other axis size in here
            // rather than passing none.
            float maxContentContribution = ItemMaxContentContributionCached(
                item, axis, tree, SizeOptF::None(), SizeOptF::None());
            itemMax = F32Max(itemMax, FindSizeOfFr(axisTracks + from, count,
                                                   maxContentContribution));
        }
        flexFraction = F32Max(trackMax, itemMax);

        // If this fraction would make the grid smaller than its min size (or
        // bigger than its max size), redo the step treating the free space as
        // definite and equal to that size. min takes precedence over max.
        float hypotheticalGridSize = 0.0f;
        for (int i = 0; i < nAxisTracks; i++) {
            const GridTrack& t = axisTracks[i];
            if (t.maxTrackSizingFunction.IsFr()) {
                hypotheticalGridSize +=
                    F32Max(t.baseSize,
                           t.maxTrackSizingFunction.raw.Value() * flexFraction);
            } else {
                hypotheticalGridSize += t.baseSize;
            }
        }
        float minSize = axisMinSize.UnwrapOr(0.0f);
        float maxSize = axisMaxSize.UnwrapOr(INFINITY);
        if (hypotheticalGridSize < minSize) {
            flexFraction = FindSizeOfFr(axisTracks, nAxisTracks, minSize);
        } else if (hypotheticalGridSize > maxSize) {
            flexFraction = FindSizeOfFr(axisTracks, nAxisTracks, maxSize);
        }
    }

    for (int i = 0; i < nAxisTracks; i++) {
        GridTrack& t = axisTracks[i];
        if (!t.maxTrackSizingFunction.IsFr()) {
            continue;
        }
        t.baseSize = F32Max(
            t.baseSize, t.maxTrackSizingFunction.raw.Value() * flexFraction);
    }
}

// 11.8 Stretch auto Tracks
void StretchAutoTracks(GridTrack* axisTracks, int n, Optf axisMinSize,
                       AvailableSpace axisAvailableSpaceForExpansion) {
    int numAutoTracks = 0;
    for (int i = 0; i < n; i++) {
        if (axisTracks[i].maxTrackSizingFunction.IsAuto()) {
            numAutoTracks++;
        }
    }
    if (numAutoTracks == 0) {
        return;
    }
    float usedSpace = 0.0f;
    for (int i = 0; i < n; i++) {
        usedSpace += axisTracks[i].baseSize;
    }
    // With indefinite free space but a definite min size, that min size is
    // what the free space is computed from.
    float freeSpace;
    if (axisAvailableSpaceForExpansion.IsDefinite()) {
        freeSpace = axisAvailableSpaceForExpansion.ComputeFreeSpace(usedSpace);
    } else {
        freeSpace = axisMinSize.IsSome() ? axisMinSize.val - usedSpace : 0.0f;
    }
    if (freeSpace > 0.0f) {
        float extra = freeSpace / (float)numAutoTracks;
        for (int i = 0; i < n; i++) {
            if (axisTracks[i].maxTrackSizingFunction.IsAuto()) {
                axisTracks[i].baseSize += extra;
            }
        }
    }
}

// The track sizing algorithm. Gutters are treated as empty fixed-size tracks.
void TrackSizingAlgorithm(TaffyTree* tree, AbstractAxis axis, Optf axisMinSize,
                          Optf axisMaxSize, AlignContent axisAlignment,
                          AlignContent otherAxisAlignment,
                          SizeAvail availableGridSpace, SizeOptF innerNodeSize,
                          GridTrack* axisTracks, int nAxisTracks,
                          GridTrack* otherAxisTracks, int nOtherAxisTracks,
                          GridItem* items, int nItems,
                          TrackSizeEstimate estimate,
                          bool hasBaselineAlignedItem) {
    // 11.4 Initialise Track sizes.
    Optf percentageBasis = innerNodeSize.Get(axis).Or(axisMinSize);
    InitializeTrackSizes(tree, axisTracks, nAxisTracks, percentageBasis);

    // 11.5.1 Shim item baselines.
    if (hasBaselineAlignedItem) {
        ResolveItemBaselines(tree, axis, items, nItems, innerNodeSize);
    }

    // Every track already at its growth limit means there is nothing to do.
    bool allAtLimit = true;
    for (int i = 0; i < nAxisTracks; i++) {
        if (axisTracks[i].baseSize != axisTracks[i].growthLimit) {
            allAtLimit = false;
            break;
        }
    }
    if (allAtLimit) {
        return;
    }

    // The extra amount added to each spanned gutter when estimating an item's
    // size in the opposite axis.
    float gutterAlignmentAdjustment = ComputeAlignmentGutterAdjustment(
        otherAxisAlignment, innerNodeSize.Get(Other(axis)), otherAxisTracks,
        nOtherAxisTracks, estimate, tree->calc);
    if (nOtherAxisTracks > 3) {
        for (int i = 2; i < nOtherAxisTracks; i += 2) {
            otherAxisTracks[i]
                .contentAlignmentAdjustment = gutterAlignmentAdjustment;
        }
    }

    // 11.5 Resolve Intrinsic Track Sizes.
    ResolveIntrinsicTrackSizes(
        tree, axis, axisTracks, nAxisTracks, otherAxisTracks, items, nItems,
        availableGridSpace.Get(axis), innerNodeSize, estimate);

    // 11.6 Maximise Tracks.
    MaximiseTracks(axisTracks, nAxisTracks, innerNodeSize.Get(axis),
                   availableGridSpace.Get(axis));

    // The last two expansion steps should only expand into space the grid
    // container's own size generates, not any available space, so definite
    // available space is mapped to max-content when the inner node size is
    // unknown.
    AvailableSpace axisAvailableSpaceForExpansion;
    Optf innerAxis = innerNodeSize.Get(axis);
    if (innerAxis.IsSome()) {
        axisAvailableSpaceForExpansion =
            AvailableSpace::Definite(innerAxis.val);
    } else if (availableGridSpace.Get(axis)
                   .kind == AvailableSpace::Kind::MinContent) {
        axisAvailableSpaceForExpansion = AvailableSpace::MinContent();
    } else {
        axisAvailableSpaceForExpansion = AvailableSpace::MaxContent();
    }

    // 11.7 Expand Flexible Tracks.
    ExpandFlexibleTracks(tree, axis, axisTracks, nAxisTracks, items, nItems,
                         axisMinSize, axisMaxSize,
                         axisAvailableSpaceForExpansion);

    // 11.8 Stretch auto Tracks.
    if (axisAlignment.keyword == AlignContentKeyword::Stretch) {
        StretchAutoTracks(axisTracks, nAxisTracks, axisMinSize,
                          axisAvailableSpaceForExpansion);
    }
}

// ─── alignment.rs ────────────────────────────────────────────────────────

// Align the tracks within the grid per align-content (rows) or
// justify-content (columns). Only does anything when the grid is a different
// size from the container in that axis.
void AlignTracks(float gridContainerContentBoxSize, LineF padding, LineF border,
                 GridTrack* tracks, int n, AlignContent trackAlignmentStyle,
                 bool axisIsReversed) {
    float usedSize = 0.0f;
    for (int i = 0; i < n; i++) {
        usedSize += tracks[i].baseSize;
    }
    float freeSpace = gridContainerContentBoxSize - usedSize;
    float origin = padding.start + border.start;

    int numTracks = 0;
    for (int i = 1; i < n; i += 2) {
        if (!tracks[i].isCollapsed) {
            numTracks++;
        }
    }

    // Grid treats gaps as full tracks rather than applying them at alignment,
    // so the gap is zero here, and grid layout is never reversed.
    float gap = 0.0f;
    bool layoutIsReversed = false;
    AlignContentKeyword trackAlignment =
        ApplyAlignmentFallback(freeSpace, numTracks, trackAlignmentStyle);
    if (axisIsReversed) {
        trackAlignment = Reversed(trackAlignment);
    }

    float totalOffset = origin;
    bool seenNonCollapsedTrack = false;
    for (int i = 0; i < n; i++) {
        GridTrack& track = tracks[i];
        // Gutters sit at even indices, since the vector starts with one.
        bool isGutter = (i % 2) == 0;
        bool isNonCollapsedTrack = !isGutter && !track.isCollapsed;
        bool isFirst = isNonCollapsedTrack && !seenNonCollapsedTrack;

        float offset = isNonCollapsedTrack
                           ? ComputeAlignmentOffset(freeSpace, numTracks, gap,
                                                    trackAlignment,
                                                    layoutIsReversed, isFirst)
                           : 0.0f;
        track.offset = totalOffset + offset;
        totalOffset = totalOffset + offset + track.baseSize;
        if (isNonCollapsedTrack) {
            seenNonCollapsedTrack = true;
        }
    }
}

struct AlignedAxis {
    float start = 0.0f;
    LineF margin;
};

// Align and size a grid item along one axis.
AlignedAxis AlignItemWithinArea(LineF gridArea, AlignSelf alignmentStyle,
                                float resolvedSize, Position position,
                                RectOptF insetLine, bool vertical,
                                RectOptF marginLine, float baselineShim,
                                Direction direction) {
    Optf insetStart = vertical ? insetLine.top : insetLine.left;
    Optf insetEnd = vertical ? insetLine.bottom : insetLine.right;
    Optf marginStart = vertical ? marginLine.top : marginLine.left;
    Optf marginEnd = vertical ? marginLine.bottom : marginLine.right;

    LineF nonAutoMargin = {marginStart.UnwrapOr(0.0f) + baselineShim,
                           marginEnd.UnwrapOr(0.0f)};
    float gridAreaSize = F32Max(gridArea.end - gridArea.start, 0.0f);
    float freeSpace =
        F32Max(gridAreaSize - resolvedSize - nonAutoMargin.Sum(), 0.0f);

    int autoMarginCount =
        (marginStart.IsSome() ? 0 : 1) + (marginEnd.IsSome() ? 0 : 1);
    float autoMarginSize =
        autoMarginCount > 0 ? freeSpace / (float)autoMarginCount : 0.0f;
    LineF resolvedMargin = {marginStart.UnwrapOr(autoMarginSize) + baselineShim,
                            marginEnd.UnwrapOr(autoMarginSize)};

    bool overflows = resolvedSize + nonAutoMargin.Sum() > gridAreaSize;
    AlignItemsKeyword keyword =
        ResolveSelfAlignmentSafety(alignmentStyle, overflows);

    float alignmentBasedOffset;
    switch (keyword) {
        case AlignItemsKeyword::End:
        case AlignItemsKeyword::FlexEnd:
            alignmentBasedOffset =
                IsRtl(direction)
                    ? resolvedMargin.start
                    : gridAreaSize - resolvedSize - resolvedMargin.end;
            break;
        case AlignItemsKeyword::Center:
            alignmentBasedOffset = (gridAreaSize - resolvedSize +
                                    resolvedMargin.start - resolvedMargin.end) /
                                   2.0f;
            break;
        default:
            // Start, FlexStart, Baseline and Stretch. Baseline alignment is
            // not supported here yet and is treated as start.
            alignmentBasedOffset =
                IsRtl(direction)
                    ? gridAreaSize - resolvedSize - resolvedMargin.end
                    : resolvedMargin.start;
            break;
    }

    float offsetWithinArea = alignmentBasedOffset;
    if (position == Position::Absolute) {
        if (insetStart.IsSome() && insetEnd.IsSome()) {
            offsetWithinArea = IsRtl(direction)
                                   ? gridAreaSize - insetEnd.val -
                                         resolvedSize - nonAutoMargin.end
                                   : insetStart.val + nonAutoMargin.start;
        } else if (insetStart.IsSome()) {
            offsetWithinArea = insetStart.val + nonAutoMargin.start;
        } else if (insetEnd.IsSome()) {
            offsetWithinArea =
                gridAreaSize - insetEnd.val - resolvedSize - nonAutoMargin.end;
        }
    }

    float start = gridArea.start + offsetWithinArea;
    if (position == Position::Relative) {
        Optf negEnd = insetEnd;
        if (negEnd.IsSome()) {
            negEnd.val = -negEnd.val;
        }
        Optf relativeInset =
            IsRtl(direction) ? negEnd.Or(insetStart) : insetStart.Or(negEnd);
        start += relativeInset.UnwrapOr(0.0f);
    }
    return {start, resolvedMargin};
}

struct AlignedItem {
    SizeF contentSizeContribution;
    float yPosition = 0.0f;
    float height = 0.0f;
};

// Align and size a grid item into its final position.
AlignedItem AlignAndPositionItem(TaffyTree* tree, NodeId node, uint32_t order,
                                 RectF gridArea,
                                 OptAlignItems containerJustifyItems,
                                 OptAlignItems containerAlignItems,
                                 float baselineShim, Direction direction) {
    CalcResolver calc = tree->calc;
    SizeF gridAreaSize = {gridArea.right - gridArea.left,
                          gridArea.bottom - gridArea.top};

    const Style& style = tree->GetStyle(node);
    PointOverflow overflow = style.overflow;
    float scrollbarWidth = style.scrollbarWidth;
    Optf aspectRatio = style.aspectRatio;
    OptAlignItems justifySelf = style.justifySelf;
    OptAlignItems alignSelf = style.alignSelf;
    Position position = style.position;

    RectOptF inset = style.inset
                         .MaybeResolveZip(AsOptional(gridAreaSize), calc);
    RectF padding = style.padding.ResolveOrZero(Optf(gridAreaSize.width), calc);
    RectF border = style.border.ResolveOrZero(Optf(gridAreaSize.width), calc);
    SizeF paddingBorderSize = (padding + border).SumAxes();
    SizeF boxSizingAdjustment = style.boxSizing == BoxSizing::ContentBox
                                    ? paddingBorderSize
                                    : SizeF::Zero();

    SizeOptF gridAreaOpt = AsOptional(gridAreaSize);
    SizeOptF inherentSize = MaybeAdd(style.size.MaybeResolve(gridAreaOpt, calc)
                                         .MaybeApplyAspectRatio(aspectRatio),
                                     boxSizingAdjustment);
    SizeOptF minSize =
        MaybeMax(MaybeAdd(style.minSize.MaybeResolve(gridAreaOpt, calc),
                          boxSizingAdjustment)
                     .Or(AsOptional(paddingBorderSize)),
                 paddingBorderSize)
            .MaybeApplyAspectRatio(aspectRatio);
    SizeOptF maxSize = MaybeAdd(style.maxSize.MaybeResolve(gridAreaOpt, calc)
                                    .MaybeApplyAspectRatio(aspectRatio),
                                boxSizingAdjustment);

    // Default alignment when neither the parent nor the item sets one. An item
    // with a preferred aspect ratio but no width or height stretches its width
    // and derives its height from the ratio.
    // https://www.w3.org/TR/css-grid-1/#grid-item-sizing
    AlignItems horizontalAlignment =
        justifySelf.Or(containerJustifyItems)
            .UnwrapOr(inherentSize.width.IsSome()
                          ? AlignItems{AlignItemsKeyword::Start}
                          : AlignItems{AlignItemsKeyword::Stretch});
    AlignItems verticalAlignment =
        alignSelf.Or(containerAlignItems)
            .UnwrapOr((inherentSize.height.IsSome() || aspectRatio.IsSome())
                          ? AlignItems{AlignItemsKeyword::Start}
                          : AlignItems{AlignItemsKeyword::Stretch});

    // Not a bug: the CSS spec has both horizontal and vertical margins resolve
    // against the *width* of the grid area.
    RectOptF margin = style.margin.MaybeResolve(Optf(gridAreaSize.width), calc);

    SizeF gridAreaMinusItemMarginsSize = {
        MaybeSub(MaybeSub(gridAreaSize.width, margin.left), margin.right),
        MaybeSub(MaybeSub(gridAreaSize.height, margin.top), margin.bottom) -
            baselineShim};

    Optf width = inherentSize.width;
    if (!width.IsSome()) {
        if (position == Position::Absolute && inset.left.IsSome() &&
            inset.right.IsSome()) {
            width = Optf(F32Max(gridAreaMinusItemMarginsSize.width -
                                    inset.left.val - inset.right.val,
                                0.0f));
        } else if (margin.left.IsSome() && margin.right.IsSome() &&
                   horizontalAlignment.keyword == AlignItemsKeyword::Stretch &&
                   position != Position::Absolute) {
            width = Optf(gridAreaMinusItemMarginsSize.width);
        }
    }
    SizeOptF sized = SizeOptF{width, inherentSize.height}
                         .MaybeApplyAspectRatio(aspectRatio);

    Optf height = sized.height;
    if (!height.IsSome()) {
        if (position == Position::Absolute && inset.top.IsSome() &&
            inset.bottom.IsSome()) {
            height = Optf(F32Max(gridAreaMinusItemMarginsSize.height -
                                     inset.top.val - inset.bottom.val,
                                 0.0f));
        } else if (margin.top.IsSome() && margin.bottom.IsSome() &&
                   verticalAlignment.keyword == AlignItemsKeyword::Stretch &&
                   position != Position::Absolute) {
            height = Optf(gridAreaMinusItemMarginsSize.height);
        }
    }
    sized = SizeOptF{sized.width, height}.MaybeApplyAspectRatio(aspectRatio);
    sized = MaybeClamp(sized, minSize, maxSize);

    SizeAvail avail = SizeAvail::Definite(gridAreaMinusItemMarginsSize);
    SizeOptF size = sized;
    if (position == Position::Absolute &&
        (!sized.width.IsSome() || !sized.height.IsSome())) {
        SizeF measured = tree->MeasureChildSizeBoth(
            node, sized, gridAreaOpt, avail, SizingMode::InherentSize,
            LineBool::False());
        size = AsOptional(measured);
    }

    LayoutOutput layoutOutput =
        tree->PerformChildLayout(node, size, gridAreaOpt, avail,
                                 SizingMode::InherentSize, LineBool::False());

    SizeF finalSize =
        MaybeClamp(size.UnwrapOr(layoutOutput.size), minSize, maxSize);

    AlignedAxis xr = AlignItemWithinArea(
        {gridArea.left, gridArea.right},
        justifySelf.UnwrapOr(horizontalAlignment), finalSize.width, position,
        inset, false, margin, 0.0f, direction);
    AlignedAxis yr = AlignItemWithinArea(
        {gridArea.top, gridArea.bottom}, alignSelf.UnwrapOr(verticalAlignment),
        finalSize.height, position, inset, true, margin, baselineShim,
        Direction::Ltr);

    SizeF scrollbarSize = {
        overflow.y == Overflow::Scroll ? scrollbarWidth : 0.0f,
        overflow.x == Overflow::Scroll ? scrollbarWidth : 0.0f};

    Layout layout;
    layout.order = order;
    layout.location = {xr.start, yr.start};
    layout.size = finalSize;
    layout.contentSize = layoutOutput.contentSize;
    layout.scrollbarSize = scrollbarSize;
    layout.padding = padding;
    layout.border = border;
    layout.margin = {xr.margin.start, xr.margin.end, yr.margin.start,
                     yr.margin.end};
    tree->SetUnroundedLayout(node, layout);

    SizeF contribution = ComputeContentSizeContribution(
        {xr.start - gridArea.left, yr.start - gridArea.top}, finalSize,
        layoutOutput.contentSize, overflow);
    return {contribution, yr.start, finalSize.height};
}

// ─── mod.rs ──────────────────────────────────────────────────────────────

// Reverses only the non-gutter column tracks, keeping the line/gutter slots.
void ReverseNonGutterTracks(GridTrack* tracks, int n, TrackCounts trackCounts) {
    // With zero or one explicit track, RTL mirroring is entirely a matter of
    // the implicit tracks, so every non-gutter track reverses.
    if (trackCounts.explicitCount <= 1) {
        const int kMinTrackVecLenToReverseColumns = 5;
        if (n < kMinTrackVecLenToReverseColumns) {
            return;
        }
        int left = 1;
        int right = n - 2;
        while (left < right) {
            GridTrack tmp = tracks[left];
            tracks[left] = tracks[right];
            tracks[right] = tmp;
            left += 2;
            right = right >= 2 ? right - 2 : 0;
        }
        return;
    }

    int explicitTrackCount = (int)trackCounts.explicitCount;
    if (explicitTrackCount < 2) {
        return;
    }
    int left = (int)trackCounts.negativeImplicit;
    int right = left + explicitTrackCount - 1;
    while (left < right) {
        int li = 2 * left + 1;
        int ri = 2 * right + 1;
        GridTrack tmp = tracks[li];
        tracks[li] = tracks[ri];
        tracks[ri] = tmp;
        left += 1;
        right = right >= 1 ? right - 1 : 0;
    }
}

// Maps an initialised column index to an occupancy-matrix index, for auto-fit
// collapsing in RTL.
int RtlColumnOccupancyIndexForInitialization(int columnIndex,
                                             TrackCounts trackCounts) {
    if (trackCounts.explicitCount <= 1) {
        return trackCounts.Len() - columnIndex - 1;
    }
    int explicitStart = (int)trackCounts.negativeImplicit;
    int explicitEnd = explicitStart + (int)trackCounts.explicitCount;
    if (columnIndex >= explicitStart && columnIndex < explicitEnd) {
        return explicitStart + (explicitEnd - columnIndex - 1);
    }
    return columnIndex;
}

} // namespace

// The CSS Grid layout algorithm, in four phases: resolve the explicit grid,
// place the items (which resolves the implicit grid), size the tracks, then
// align and place.
LayoutOutput ComputeGridLayout(TaffyTree* tree, NodeId node,
                               const LayoutInput& inputs) {
    CalcResolver calc = tree->calc;
    SizeOptF knownDimensions = inputs.knownDimensions;
    SizeOptF parentSize = inputs.parentSize;
    SizeAvail availableSpace = inputs.availableSpace;
    RunMode runMode = inputs.runMode;

    const Style& style = tree->GetStyle(node);
    Direction direction = style.direction;

    // 1. Available grid space.
    // https://www.w3.org/TR/css-grid-1/#available-grid-space
    Optf aspectRatio = style.aspectRatio;
    RectF padding = style.padding.ResolveOrZero(parentSize.width, calc);
    RectF border = style.border.ResolveOrZero(parentSize.width, calc);
    RectF paddingBorder = padding + border;
    SizeF paddingBorderSize = paddingBorder.SumAxes();
    SizeF boxSizingAdjustment = style.boxSizing == BoxSizing::ContentBox
                                    ? paddingBorderSize
                                    : SizeF::Zero();

    SizeOptF minSize = MaybeAdd(style.minSize.MaybeResolve(parentSize, calc)
                                    .MaybeApplyAspectRatio(aspectRatio),
                                boxSizingAdjustment);
    SizeOptF maxSize = MaybeAdd(style.maxSize.MaybeResolve(parentSize, calc)
                                    .MaybeApplyAspectRatio(aspectRatio),
                                boxSizingAdjustment);
    SizeOptF preferredSize;
    if (inputs.sizingMode == SizingMode::InherentSize) {
        preferredSize = MaybeAdd(style.size.MaybeResolve(parentSize, calc)
                                     .MaybeApplyAspectRatio(aspectRatio),
                                 boxSizingAdjustment);
    }

    // A node that scrolls vertically needs *horizontal* space reserved for its
    // scrollbar, hence the transposed axes.
    PointOverflow t = style.overflow.Transpose();
    PointF scrollbarGutter = {
        t.x == Overflow::Scroll ? style.scrollbarWidth : 0.0f,
        t.y == Overflow::Scroll ? style.scrollbarWidth : 0.0f};
    RectF contentBoxInset = paddingBorder;
    contentBoxInset.bottom += scrollbarGutter.y;
    if (direction == Direction::Ltr) {
        contentBoxInset.right += scrollbarGutter.x;
    } else {
        contentBoxInset.left += scrollbarGutter.x;
    }

    AlignContent alignContent =
        style.alignContent.UnwrapOr(AlignContent{AlignContentKeyword::Stretch});
    AlignContent justifyContent =
        style.justifyContent
            .UnwrapOr(AlignContent{AlignContentKeyword::Stretch});
    OptAlignItems alignItems = style.alignItems;
    OptAlignItems justifyItems = style.justifyItems;

    SizeOptF sizeOrPreferred = knownDimensions.Or(preferredSize);
    // The available space the grid is sized against: a known or preferred
    // size wins over what the parent offered, then min/max clamp it and the
    // padding+border floor it.
    SizeAvail constrainedAvailableSpace = availableSpace;
    if (sizeOrPreferred.width.IsSome()) {
        constrainedAvailableSpace
            .width = AvailableSpace::Definite(sizeOrPreferred.width.val);
    }
    if (sizeOrPreferred.height.IsSome()) {
        constrainedAvailableSpace
            .height = AvailableSpace::Definite(sizeOrPreferred.height.val);
    }
    constrainedAvailableSpace =
        MaybeClamp(constrainedAvailableSpace, minSize, maxSize);
    constrainedAvailableSpace.width =
        MaybeMax(constrainedAvailableSpace.width, paddingBorderSize.width);
    constrainedAvailableSpace.height =
        MaybeMax(constrainedAvailableSpace.height, paddingBorderSize.height);

    SizeAvail availableGridSpace = constrainedAvailableSpace;
    if (availableGridSpace.width.IsDefinite()) {
        availableGridSpace.width =
            AvailableSpace::Definite(availableGridSpace.width.value -
                                     contentBoxInset.HorizontalAxisSum());
    }
    if (availableGridSpace.height.IsDefinite()) {
        availableGridSpace.height =
            AvailableSpace::Definite(availableGridSpace.height.value -
                                     contentBoxInset.VerticalAxisSum());
    }

    SizeOptF outerNodeSize = MaybeMax(
        MaybeClamp(sizeOrPreferred, minSize, maxSize), paddingBorderSize);
    SizeOptF innerNodeSize = {
        MaybeSub(outerNodeSize.width, contentBoxInset.HorizontalAxisSum()),
        MaybeSub(outerNodeSize.height, contentBoxInset.VerticalAxisSum())};

    if (runMode == RunMode::ComputeSize) {
        if (outerNodeSize.BothAxisDefined()) {
            return LayoutOutput::FromOuterSize(
                {outerNodeSize.width.val, outerNodeSize.height.val});
        }
        if (inputs.axis == RequestedAxis::Horizontal && outerNodeSize.width
                                                            .IsSome()) {
            return LayoutOutput::FromOuterSize({outerNodeSize.width.val, 0.0f});
        }
    }

    // 2. Resolve the explicit grid.
    //
    // Like innerNodeSize, except that an indefinite inner size falls back to a
    // min- or max-size style if the node has one.
    SizeOptF autoFitContainerSize = MaybeSub(
        MaybeMax(
            MaybeClamp(outerNodeSize.Or(maxSize).Or(minSize), minSize, maxSize),
            paddingBorderSize),
        contentBoxInset.SumAxes());

    // With a definite size or max size, the number of repetitions is the
    // largest that does not overflow; with only a definite min size, the
    // smallest that meets it; otherwise the list repeats once.
    AutoRepeatStrategy colStrategy =
        (outerNodeSize.width.IsSome() || maxSize.width.IsSome())
            ? AutoRepeatStrategy::MaxRepetitionsThatDoNotOverflow
            : AutoRepeatStrategy::MinRepetitionsThatDoOverflow;
    AutoRepeatStrategy rowStrategy =
        (outerNodeSize.height.IsSome() || maxSize.height.IsSome())
            ? AutoRepeatStrategy::MaxRepetitionsThatDoNotOverflow
            : AutoRepeatStrategy::MinRepetitionsThatDoOverflow;

    ExplicitGridSize colSize = ComputeExplicitGridSizeInAxis(
        style, autoFitContainerSize.width, colStrategy, calc,
        AbsoluteAxis::Horizontal);
    ExplicitGridSize rowSize = ComputeExplicitGridSizeInAxis(
        style, autoFitContainerSize.height, rowStrategy, calc,
        AbsoluteAxis::Vertical);

    NamedLineResolver nameResolver;
    nameResolver
        .Init(style, colSize.autoRepetitionCount, rowSize.autoRepetitionCount);

    uint16_t explicitColCount = colSize.trackCount > nameResolver
                                                         .areaColumnCount
                                    ? colSize.trackCount
                                    : nameResolver.areaColumnCount;
    uint16_t explicitRowCount = rowSize.trackCount > nameResolver.areaRowCount
                                    ? rowSize.trackCount
                                    : nameResolver.areaRowCount;
    nameResolver.explicitColumnCount = explicitColCount;
    nameResolver.explicitRowCount = explicitRowCount;

    // The in-flow children, with their names already resolved.
    Vec<PlacementChild> children;
    Vec<ChildPlacementStyles> childPlacements;
    int childCount = tree->ChildCount(node);

    for (int i = 0; i < childCount; i++) {
        NodeId child = tree->GetChildId(node, i);
        const Style& cs = tree->GetStyle(child);
        if (cs.BoxGenMode() == BoxGenerationMode::None ||
            cs.position == Position::Absolute) {
            continue;
        }
        PlacementChild pc;
        pc.index = i;
        pc.node = child;
        pc.horizontal = nameResolver.ResolveColumnNames(cs.gridColumn)
                            .IntoOriginZero(explicitColCount);
        pc.vertical = nameResolver.ResolveRowNames(cs.gridRow)
                          .IntoOriginZero(explicitRowCount);
        children.Append(pc);
        childPlacements.Append({cs.gridColumn, cs.gridRow});
    }

    // 3. Estimate the implicit track counts, which pre-sizes the occupancy
    //    matrix and is a necessary step in auto-placement.
    TrackCounts estColCounts;
    TrackCounts estRowCounts;
    ComputeGridSizeEstimate(explicitColCount, explicitRowCount, direction,
                            childPlacements.els, childPlacements.len,
                            &estColCounts, &estRowCounts);

    // 4. Grid item placement.
    Vec<GridItem> items;
    CellOccupancyMatrix cellOccupancyMatrix;
    cellOccupancyMatrix.Init(estColCounts, estRowCounts);
    PlaceGridItems(&cellOccupancyMatrix, &items, tree, children, direction,
                   style.gridAutoFlow,
                   alignItems.UnwrapOr(AlignItems{AlignItemsKeyword::Stretch}),
                   justifyItems
                       .UnwrapOr(AlignItems{AlignItemsKeyword::Stretch}));

    // Auto-placement can add tracks, so the counts are read back here.
    TrackCounts finalColCounts = cellOccupancyMatrix
                                     .Counts(AbsoluteAxis::Horizontal);
    TrackCounts finalRowCounts = cellOccupancyMatrix
                                     .Counts(AbsoluteAxis::Vertical);

    // 5. Initialise the tracks and gutters, resolving their sizing functions.
    Vec<GridTrack> columns;
    Vec<GridTrack> rows;
    TrackCounts columnTrackCountsForInit = finalColCounts;
    if (IsRtl(direction) && finalColCounts.explicitCount <= 1) {
        columnTrackCountsForInit.negativeImplicit = finalColCounts
                                                        .positiveImplicit;
        columnTrackCountsForInit.positiveImplicit = finalColCounts
                                                        .negativeImplicit;
    }
    InitializeGridTracks(
        &columns, columnTrackCountsForInit, style, AbsoluteAxis::Horizontal,
        [&](int columnIndex) {
            int occupancyIndex = IsRtl(direction)
                                     ? RtlColumnOccupancyIndexForInitialization(
                                           columnIndex, finalColCounts)
                                     : columnIndex;
            return cellOccupancyMatrix.ColumnIsOccupied(occupancyIndex);
        });
    InitializeGridTracks(&rows, finalRowCounts, style, AbsoluteAxis::Vertical,
                         [&](int rowIndex) {
                             return cellOccupancyMatrix.RowIsOccupied(rowIndex);
                         });
    if (IsRtl(direction)) {
        ReverseNonGutterTracks(columns.els, columns.len, finalColCounts);
    }

    // 6. Track sizing.
    ResolveItemTrackIndexes(items.els, items.len, finalColCounts,
                            finalRowCounts);
    DetermineIfItemCrossesFlexibleOrIntrinsicTracks(items.els, items.len,
                                                    columns.els, rows.els);

    bool hasBaselineAlignedItem = false;
    for (int i = 0; i < items.len; i++) {
        if (items[i].alignSelf.keyword == AlignItemsKeyword::Baseline) {
            hasBaselineAlignedItem = true;
            break;
        }
    }

    TrackSizingAlgorithm(
        tree, AbstractAxis::Inline, minSize.Get(AbstractAxis::Inline),
        maxSize.Get(AbstractAxis::Inline), justifyContent, alignContent,
        availableGridSpace, innerNodeSize, columns.els, columns.len, rows.els,
        rows.len, items.els, items.len,
        TrackSizeEstimate::MaxTrackSizingFunction, hasBaselineAlignedItem);
    float initialColumnSum = 0.0f;
    for (int i = 0; i < columns.len; i++) {
        initialColumnSum += columns[i].baseSize;
    }
    if (!innerNodeSize.width.IsSome()) {
        innerNodeSize.width = Optf(initialColumnSum);
    }

    for (int i = 0; i < items.len; i++) {
        items[i].hasGridAreaSizeCache = false;
    }

    TrackSizingAlgorithm(
        tree, AbstractAxis::Block, minSize.Get(AbstractAxis::Block),
        maxSize.Get(AbstractAxis::Block), alignContent, justifyContent,
        availableGridSpace, innerNodeSize, rows.els, rows.len, columns.els,
        columns.len, items.els, items.len, TrackSizeEstimate::BaseSize,
        // TODO(taffy): baseline alignment in the block axis.
        false);
    float initialRowSum = 0.0f;
    for (int i = 0; i < rows.len; i++) {
        initialRowSum += rows[i].baseSize;
    }
    if (!innerNodeSize.height.IsSome()) {
        innerNodeSize.height = Optf(initialRowSum);
    }

    // 6. Container size.
    SizeOptF resolvedStyleSize = knownDimensions.Or(preferredSize);
    SizeF containerBorderBox = {
        F32Max(MaybeClamp(resolvedStyleSize.Get(AbstractAxis::Inline)
                              .UnwrapOr(initialColumnSum +
                                        contentBoxInset.HorizontalAxisSum()),
                          minSize.width, maxSize.width),
               paddingBorderSize.width),
        F32Max(MaybeClamp(resolvedStyleSize.Get(AbstractAxis::Block)
                              .UnwrapOr(initialRowSum + contentBoxInset
                                                            .VerticalAxisSum()),
                          minSize.height, maxSize.height),
               paddingBorderSize.height)};
    SizeF containerContentBox = {
        F32Max(0.0f, containerBorderBox.width - contentBoxInset
                                                    .HorizontalAxisSum()),
        F32Max(0.0f, containerBorderBox.height - contentBoxInset
                                                     .VerticalAxisSum())};

    if (runMode == RunMode::ComputeSize) {
        items.Reset();
        columns.Reset();
        rows.Reset();
        children.Reset();
        childPlacements.Reset();
        cellOccupancyMatrix.Free();
        nameResolver.Free();
        return LayoutOutput::FromOuterSize(containerBorderBox);
    }

    // 7. Re-resolve percentage track base sizes. An indefinitely sized
    // container resolves them to zero during "Initialise Tracks", so they are
    // resolved again here against the content-sized content box.
    if (!availableGridSpace.width.IsDefinite()) {
        for (int i = 0; i < columns.len; i++) {
            GridTrack& c = columns[i];
            Optf mn = c.minTrackSizingFunction.ResolvedPercentageSize(
                containerContentBox.width, calc);
            Optf mx = c.maxTrackSizingFunction.ResolvedPercentageSize(
                containerContentBox.width, calc);
            c.baseSize = MaybeClamp(c.baseSize, mn, mx);
        }
    }
    if (!availableGridSpace.height.IsDefinite()) {
        for (int i = 0; i < rows.len; i++) {
            GridTrack& r = rows[i];
            Optf mn = r.minTrackSizingFunction.ResolvedPercentageSize(
                containerContentBox.height, calc);
            Optf mx = r.maxTrackSizingFunction.ResolvedPercentageSize(
                containerContentBox.height, calc);
            r.baseSize = MaybeClamp(r.baseSize, mn, mx);
        }
    }

    // Column sizing is re-run once if the container's width was indefinite and
    // there are percentage columns, or if any item's min-content contribution
    // across an intrinsic track changed.
    bool hasPercentageColumn = false;
    for (int i = 0; i < columns.len; i++) {
        if (columns[i].UsesPercentage()) {
            hasPercentageColumn = true;
            break;
        }
    }
    bool hasPercentageRow = false;
    for (int i = 0; i < rows.len; i++) {
        if (rows[i].UsesPercentage()) {
            hasPercentageRow = true;
            break;
        }
    }
    bool parentWidthIndefinite = !availableSpace.width.IsDefinite();
    bool rerunColumnSizing = parentWidthIndefinite && hasPercentageColumn;
    bool intrinsicColumnContributionChanged = false;

    if (!rerunColumnSizing) {
        for (int i = 0; i < items.len; i++) {
            GridItem* item = &items[i];
            if (!item->crossesIntrinsicColumn) {
                continue;
            }
            SizeOptF gridAreaSize = ItemGridAreaSize(
                *item, AbstractAxis::Inline, columns.els, rows.els,
                innerNodeSize, TrackSizeEstimate::BaseSize, calc);
            SizeOptF avail = gridAreaSize;
            avail.Set(AbstractAxis::Inline, Optf());
            float newMinContent = ItemMinContentContribution(
                *item, AbstractAxis::Inline, tree, gridAreaSize, avail);
            bool changed =
                !(item->minContentContributionCache.width.IsSome() &&
                  item->minContentContributionCache.width.val == newMinContent);
            item->gridAreaSizeCache = gridAreaSize;
            item->hasGridAreaSizeCache = true;
            item->minContentContributionCache.width = Optf(newMinContent);
            item->maxContentContributionCache.width = Optf();
            item->minimumContributionCache.width = Optf();
            if (changed) {
                intrinsicColumnContributionChanged = true;
            }
        }
        rerunColumnSizing = intrinsicColumnContributionChanged;
    } else {
        for (int i = 0; i < items.len; i++) {
            items[i].hasGridAreaSizeCache = false;
            items[i].minContentContributionCache.width = Optf();
            items[i].maxContentContributionCache.width = Optf();
            items[i].minimumContributionCache.width = Optf();
        }
    }

    bool intrinsicRowContributionChanged = false;
    if (rerunColumnSizing) {
        TrackSizingAlgorithm(
            tree, AbstractAxis::Inline, minSize.Get(AbstractAxis::Inline),
            maxSize.Get(AbstractAxis::Inline), justifyContent, alignContent,
            availableGridSpace, innerNodeSize, columns.els, columns.len,
            rows.els, rows.len, items.els, items.len,
            TrackSizeEstimate::BaseSize, hasBaselineAlignedItem);

        bool parentHeightIndefinite = !availableSpace.height.IsDefinite();
        bool rerunRowSizing = parentHeightIndefinite && hasPercentageRow;

        if (!rerunRowSizing) {
            for (int i = 0; i < items.len; i++) {
                GridItem* item = &items[i];
                if (!item->crossesIntrinsicColumn) {
                    continue;
                }
                SizeOptF gridAreaSize = ItemGridAreaSize(
                    *item, AbstractAxis::Block, rows.els, columns.els,
                    innerNodeSize, TrackSizeEstimate::BaseSize, calc);
                SizeOptF avail = gridAreaSize;
                avail.Set(AbstractAxis::Block, Optf());
                float newMinContent = ItemMinContentContribution(
                    *item, AbstractAxis::Block, tree, gridAreaSize, avail);
                bool changed =
                    !(item->minContentContributionCache.height.IsSome() &&
                      item->minContentContributionCache.height
                              .val == newMinContent);
                item->gridAreaSizeCache = gridAreaSize;
                item->hasGridAreaSizeCache = true;
                item->minContentContributionCache.height = Optf(newMinContent);
                item->maxContentContributionCache.height = Optf();
                item->minimumContributionCache.height = Optf();
                if (changed) {
                    intrinsicRowContributionChanged = true;
                }
            }
            rerunRowSizing = intrinsicRowContributionChanged;
        } else {
            for (int i = 0; i < items.len; i++) {
                items[i].hasGridAreaSizeCache = false;
                items[i].minContentContributionCache.height = Optf();
                items[i].maxContentContributionCache.height = Optf();
                items[i].minimumContributionCache.height = Optf();
            }
        }

        if (rerunRowSizing) {
            TrackSizingAlgorithm(
                tree, AbstractAxis::Block, minSize.Get(AbstractAxis::Block),
                maxSize.Get(AbstractAxis::Block), alignContent, justifyContent,
                availableGridSpace, innerNodeSize, rows.els, rows.len,
                columns.els, columns.len, items.els, items.len,
                TrackSizeEstimate::BaseSize, false);
        }
    }

    if ((intrinsicColumnContributionChanged && !hasPercentageColumn) ||
        (intrinsicRowContributionChanged && !hasPercentageRow)) {
        float finalColumnSum = 0.0f;
        for (int i = 0; i < columns.len; i++) {
            finalColumnSum += columns[i].baseSize;
        }
        float finalRowSum = 0.0f;
        for (int i = 0; i < rows.len; i++) {
            finalRowSum += rows[i].baseSize;
        }
        if (intrinsicColumnContributionChanged && !hasPercentageColumn) {
            containerBorderBox.width = F32Max(
                MaybeClamp(resolvedStyleSize.Get(AbstractAxis::Inline)
                               .UnwrapOr(finalColumnSum +
                                         contentBoxInset.HorizontalAxisSum()),
                           minSize.width, maxSize.width),
                paddingBorderSize.width);
            containerContentBox
                .width = F32Max(0.0f, containerBorderBox.width -
                                          contentBoxInset.HorizontalAxisSum());
        }
        if (intrinsicRowContributionChanged && !hasPercentageRow) {
            containerBorderBox.height = F32Max(
                MaybeClamp(resolvedStyleSize.Get(AbstractAxis::Block)
                               .UnwrapOr(finalRowSum + contentBoxInset
                                                           .VerticalAxisSum()),
                           minSize.height, maxSize.height),
                paddingBorderSize.height);
            containerContentBox
                .height = F32Max(0.0f, containerBorderBox.height -
                                           contentBoxInset.VerticalAxisSum());
        }
    }

    // 8. Track alignment.
    float inlineSizeWithoutScrollbar =
        F32Max(containerBorderBox.width - paddingBorderSize.width, 0.0f);
    float inlineScrollbarGutterForAlignment =
        F32Min(scrollbarGutter.x, inlineSizeWithoutScrollbar);
    AlignTracks(
        containerContentBox.Get(AbstractAxis::Inline),
        {padding.left +
             (IsRtl(direction) ? inlineScrollbarGutterForAlignment : 0.0f),
         padding.right +
             (IsRtl(direction) ? 0.0f : inlineScrollbarGutterForAlignment)},
        {border.left, border.right}, columns.els, columns.len, justifyContent,
        IsRtl(direction));
    AlignTracks(containerContentBox.Get(AbstractAxis::Block),
                {padding.top, padding.bottom}, {border.top, border.bottom},
                rows.els, rows.len, alignContent, false);

    // 9. Size, align and position the grid items.
    SizeF itemContentSizeContribution = SizeF::Zero();

    // Back into source order, so items line up with their styles again.
    StableSort(items.els, items.len, [](const GridItem& a, const GridItem& b) {
        return a.sourceOrder < b.sourceOrder;
    });

    for (int index = 0; index < items.len; index++) {
        GridItem& item = items[index];
        RectF gridArea = {columns[(int)item.columnIndexes.start + 1].offset,
                          columns[(int)item.columnIndexes.end].offset,
                          rows[(int)item.rowIndexes.start + 1].offset,
                          rows[(int)item.rowIndexes.end].offset};
        AlignedItem placed = AlignAndPositionItem(
            tree, item.node, (uint32_t)index, gridArea, justifyItems,
            alignItems, item.baselineShim, direction);
        item.yPosition = placed.yPosition;
        item.height = placed.height;
        itemContentSizeContribution = itemContentSizeContribution
                                          .Max(placed.contentSizeContribution);
    }

    // Hidden and absolutely positioned children.
    uint32_t order = (uint32_t)items.len;
    for (int index = 0; index < childCount; index++) {
        NodeId child = tree->GetChildId(node, index);
        const Style& cs = tree->GetStyle(child);

        if (cs.BoxGenMode() == BoxGenerationMode::None) {
            tree->SetUnroundedLayout(child, Layout::WithOrder(order));
            tree->PerformChildLayout(child, SizeOptF::None(), SizeOptF::None(),
                                     SizeAvail::MaxContent(),
                                     SizingMode::InherentSize,
                                     LineBool::False());
            order += 1;
            continue;
        }
        if (cs.position != Position::Absolute) {
            continue;
        }

        // grid-column-{start,end} as optional indexes into the column vector.
        // A None is an auto or an unresolvable span.
        LineOptOzl colLines = nameResolver.ResolveColumnNames(cs.gridColumn)
                                  .IntoOriginZero(finalColCounts.explicitCount)
                                  .ResolveAbsolutelyPositionedGridTracks();
        int colStartIdx = -1;
        int colEndIdx = -1;
        if (colLines.start.IsSome()) {
            OriginZeroLine l = colLines.start.val;
            if (IsRtl(direction)) {
                l = OriginZeroLine{
                    (int16_t)((int16_t)finalColCounts.explicitCount - l.v)};
            }
            TryIntoTrackVecIndex(l, finalColCounts, &colStartIdx);
        }
        if (colLines.end.IsSome()) {
            OriginZeroLine l = colLines.end.val;
            if (IsRtl(direction)) {
                l = OriginZeroLine{
                    (int16_t)((int16_t)finalColCounts.explicitCount - l.v)};
            }
            TryIntoTrackVecIndex(l, finalColCounts, &colEndIdx);
        }
        if (IsRtl(direction)) {
            int tmp = colStartIdx;
            colStartIdx = colEndIdx;
            colEndIdx = tmp;
        }

        LineOptOzl rowLines = nameResolver.ResolveRowNames(cs.gridRow)
                                  .IntoOriginZero(finalRowCounts.explicitCount)
                                  .ResolveAbsolutelyPositionedGridTracks();
        int rowStartIdx = -1;
        int rowEndIdx = -1;
        if (rowLines.start.IsSome()) {
            TryIntoTrackVecIndex(rowLines.start.val, finalRowCounts,
                                 &rowStartIdx);
        }
        if (rowLines.end.IsSome()) {
            TryIntoTrackVecIndex(rowLines.end.val, finalRowCounts, &rowEndIdx);
        }

        RectF gridArea;
        gridArea.top = rowStartIdx >= 0 ? rows[rowStartIdx].offset : border.top;
        gridArea.bottom =
            rowEndIdx >= 0
                ? rows[rowEndIdx].offset
                : containerBorderBox.height - border.bottom - scrollbarGutter.y;
        gridArea
            .left = colStartIdx >= 0
                        ? columns[colStartIdx].offset
                        : (IsRtl(direction) ? border.left + scrollbarGutter.x
                                            : border.left);
        gridArea.right =
            colEndIdx >= 0
                ? columns[colEndIdx].offset
                : (IsRtl(direction) ? containerBorderBox.width - border.right
                                    : containerBorderBox.width - border.right -
                                          scrollbarGutter.x);

        // TODO(taffy): baseline alignment for absolutely positioned items.
        AlignedItem placed =
            AlignAndPositionItem(tree, child, order, gridArea, justifyItems,
                                 alignItems, 0.0f, direction);
        itemContentSizeContribution = itemContentSizeContribution
                                          .Max(placed.contentSizeContribution);
        order += 1;
    }

    LayoutOutput out;
    if (items.len == 0) {
        out = LayoutOutput::FromOuterSize(containerBorderBox);
    } else {
        // The grid container's first baseline. Sort by row start so the items
        // of the first row are contiguous.
        StableSort(items.els, items.len,
                   [](const GridItem& a, const GridItem& b) {
                       return a.row.start < b.row.start;
                   });
        OriginZeroLine firstRow = items[0].row.start;
        int rowEnd = 0;
        while (rowEnd < items.len && items[rowEnd].row.start == firstRow) {
            rowEnd++;
        }
        const GridItem* chosen = &items[0];
        for (int i = 0; i < rowEnd; i++) {
            if (items[i].alignSelf.keyword == AlignItemsKeyword::Baseline) {
                chosen = &items[i];
                break;
            }
        }
        float gridContainerBaseline =
            chosen->yPosition + chosen->baseline.UnwrapOr(chosen->height);
        out = LayoutOutput::FromSizesAndBaselines(
            containerBorderBox, itemContentSizeContribution,
            PointOptF{Optf(), Optf(gridContainerBaseline)});
    }

    items.Reset();
    columns.Reset();
    rows.Reset();
    children.Reset();
    childPlacements.Reset();
    cellOccupancyMatrix.Free();
    nameResolver.Free();
    return out;
}

// ─── test seams ──────────────────────────────────────────────────────────
//
// See compute.h: these exist so tests/TaffyTests.cpp can reach the three
// internals taffy's own unit tests cover, and have no other caller.

void GridExplicitSizeForTest(const Style& style, Optf autoFitContainerSize,
                             bool maxRepetitions, AbsoluteAxis axis,
                             CalcResolver calc, uint16_t* outAutoRepetitions,
                             uint16_t* outTrackCount) {
    ExplicitGridSize r = ComputeExplicitGridSizeInAxis(
        style, autoFitContainerSize,
        maxRepetitions ? AutoRepeatStrategy::MaxRepetitionsThatDoNotOverflow
                       : AutoRepeatStrategy::MinRepetitionsThatDoOverflow,
        calc, axis);
    *outAutoRepetitions = r.autoRepetitionCount;
    *outTrackCount = r.trackCount;
}

void GridChildMinMaxSpanForTest(LinePlacement line, uint16_t explicitTrackCount,
                                int16_t* outMinLine, int16_t* outMaxLine,
                                uint16_t* outSpan) {
    MinMaxSpan r = ChildMinLineMaxLineSpan(line, explicitTrackCount);
    *outMinLine = r.minLine.v;
    *outMaxLine = r.maxLine.v;
    *outSpan = r.span;
}

void GridSizeEstimateForTest(uint16_t explicitColCount,
                             uint16_t explicitRowCount, Direction direction,
                             const LinePlacement* columns,
                             const LinePlacement* rows, int n,
                             uint16_t* outColCounts, uint16_t* outRowCounts) {
    Vec<ChildPlacementStyles> children;
    for (int i = 0; i < n; i++) {
        children.Append({columns[i], rows[i]});
    }
    TrackCounts cols;
    TrackCounts rws;
    ComputeGridSizeEstimate(explicitColCount, explicitRowCount, direction,
                            children.els, children.len, &cols, &rws);
    children.Reset();
    outColCounts[0] = cols.negativeImplicit;
    outColCounts[1] = cols.explicitCount;
    outColCounts[2] = cols.positiveImplicit;
    outRowCounts[0] = rws.negativeImplicit;
    outRowCounts[1] = rws.explicitCount;
    outRowCounts[2] = rws.positiveImplicit;
}

int GridInitTracksForTest(const Style& style, AbsoluteAxis axis,
                          uint16_t negativeImplicit, uint16_t explicitCount,
                          uint16_t positiveImplicit, GridTrackForTest* out,
                          int cap) {
    Vec<GridTrack> tracks;
    TrackCounts counts =
        TrackCounts::FromRaw(negativeImplicit, explicitCount, positiveImplicit);
    InitializeGridTracks(&tracks, counts, style, axis,
                         [](int) { return false; });
    int n = tracks.len;
    for (int i = 0; i < n && i < cap; i++) {
        out[i].isGutter = tracks[i].kind == GridTrackKind::Gutter;
        out[i].isCollapsed = tracks[i].isCollapsed;
        out[i].min = tracks[i].minTrackSizingFunction.raw;
        out[i].max = tracks[i].maxTrackSizingFunction.raw;
    }
    tracks.Reset();
    return n;
}

int GridPlaceForTest(TaffyTree* tree, NodeId parent, uint16_t explicitColCount,
                     uint16_t explicitRowCount, GridAutoFlow flow,
                     GridPlacementForTest* out, int cap, uint16_t* outColCounts,
                     uint16_t* outRowCounts) {
    NamedLineResolver nameResolver;
    nameResolver.Init(tree->GetStyle(parent), 0, 0);
    nameResolver.explicitColumnCount = explicitColCount;
    nameResolver.explicitRowCount = explicitRowCount;

    Vec<PlacementChild> children;
    Vec<ChildPlacementStyles> childPlacements;
    int childCount = tree->ChildCount(parent);
    for (int i = 0; i < childCount; i++) {
        NodeId child = tree->GetChildId(parent, i);
        const Style& cs = tree->GetStyle(child);
        if (cs.BoxGenMode() == BoxGenerationMode::None ||
            cs.position == Position::Absolute) {
            continue;
        }
        PlacementChild pc;
        pc.index = i;
        pc.node = child;
        pc.horizontal = nameResolver.ResolveColumnNames(cs.gridColumn)
                            .IntoOriginZero(explicitColCount);
        pc.vertical = nameResolver.ResolveRowNames(cs.gridRow)
                          .IntoOriginZero(explicitRowCount);
        children.Append(pc);
        childPlacements.Append({cs.gridColumn, cs.gridRow});
    }

    TrackCounts estCols;
    TrackCounts estRows;
    ComputeGridSizeEstimate(explicitColCount, explicitRowCount, Direction::Ltr,
                            childPlacements.els, childPlacements.len, &estCols,
                            &estRows);

    Vec<GridItem> items;
    CellOccupancyMatrix matrix;
    matrix.Init(estCols, estRows);
    PlaceGridItems(&matrix, &items, tree, children, Direction::Ltr, flow,
                   AlignItems{AlignItemsKeyword::Start},
                   AlignItems{AlignItemsKeyword::Start});

    int n = items.len;
    for (int i = 0; i < n && i < cap; i++) {
        out[i].columnStart = items[i].column.start.v;
        out[i].columnEnd = items[i].column.end.v;
        out[i].rowStart = items[i].row.start.v;
        out[i].rowEnd = items[i].row.end.v;
    }
    TrackCounts cols = matrix.Counts(AbsoluteAxis::Horizontal);
    TrackCounts rws = matrix.Counts(AbsoluteAxis::Vertical);
    outColCounts[0] = cols.negativeImplicit;
    outColCounts[1] = cols.explicitCount;
    outColCounts[2] = cols.positiveImplicit;
    outRowCounts[0] = rws.negativeImplicit;
    outRowCounts[1] = rws.explicitCount;
    outRowCounts[2] = rws.positiveImplicit;

    items.Reset();
    children.Reset();
    childPlacements.Reset();
    matrix.Free();
    nameResolver.Free();
    return n;
}

} // namespace taffy
