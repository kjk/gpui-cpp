/* Plot helpers — crates/ui/src/plot
   The d3 scales Chart maps data through; AreaChart is the render entry.

   Rust's scales are generic over the domain and range types. Here the domain
   is always float — that is what a chart axis carries — and ScaleOrdinal maps
   indexes rather than values, since what it is for is picking one of N series
   colors by position. Domains and ranges are borrowed (pointer + length), so
   nothing here allocates. */

#include "ui/chart.h"

namespace gpui {

namespace component {

// ScaleLinear — https://d3js.org/d3-scale/linear
//
// The domain collapses to its min and max, and so does the range, except that
// the range keeps the order it was given in: a range that starts high and ends
// low maps the domain backwards, which is how a y axis is drawn.
struct ScaleLinear {
    int domainLen = 0;
    float domainStart = 0;
    float domainDiff = 0;
    float rangeStart = 0;
    float rangeDiff = 0;

    static ScaleLinear New(const float* domain, int domainN, const float* range,
                           int rangeN);
    // The range position of `value`, or false when the domain has no extent to
    // divide by — Rust's `Option<f32>`.
    bool Tick(float value, float* out) const;
    // The domain entry whose tick is nearest `tick`, and that tick. Returns
    // (0, 0) for an empty domain.
    void LeastIndexWithDomain(float tick, const float* domain, int domainN,
                              int* outIndex, float* outTick) const;
};

// ScalePoint — https://d3js.org/d3-scale/point
//
// Discrete domain values spread evenly across the range, the first and last
// landing on its ends. A one-value domain sits in the middle instead.
struct ScalePoint {
    const float* domain = nullptr;
    int domainLen = 0;
    float rangeStart = 0;
    float rangeTick = 0;

    static ScalePoint New(const float* domain, int domainN, const float* range,
                          int rangeN);
    // False when `value` is not in the domain.
    bool Tick(float value, float* out) const;
    // The domain index nearest `tick`.
    int LeastIndex(float tick) const;
};

// ScaleBand — https://d3js.org/d3-scale/band
//
// A bar chart's x axis: the domain's entries each take a band of the range,
// with padding between them and at the ends. Rust's domain is a vector of
// values; here it is a count, since a band is picked by index — which is what
// a caller walking its data already has.
struct ScaleBand {
    int domainLen = 0;
    float rangeDiff = 0;
    float avgWidth = 0;
    float paddingInner = 0;
    float paddingOuter = 0;

    static ScaleBand New(int domainN, const float* range, int rangeN);
    // band_width: what one band is drawn at, which Rust caps at thirty.
    float BandWidth() const;
    // The range position of the band at `index`, or false when it is not one
    // of them. A one-band domain sits in the middle of the range.
    bool Tick(int index, float* out) const;
    // The band nearest `tick`, clamped to the domain.
    int LeastIndex(float tick) const;
};

// ScaleOrdinal — https://d3js.org/d3-scale/ordinal
//
// Rust maps a domain value to a range value; this maps the domain *index*,
// which is what the caller already has when it is handing out per-series
// colors. The range cycles when it is shorter than the domain.
struct ScaleOrdinal {
    int rangeLen = 0;
    // The range index for a value that is not in the domain: Rust's
    // `unknown()`, and -1 for its `None`.
    int unknown = -1;

    // `domainIndex` < 0 means the value was not in the domain. Returns the
    // range index, or -1 when there is nothing to map onto.
    int Map(int domainIndex) const;
};

} // namespace component
} // namespace gpui
