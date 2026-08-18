#include "component/Plot.h"

#include <math.h>

namespace gpui {

namespace component {

static void MinMax(const float* v, int n, float* outMin, float* outMax) {
    if (n <= 0) {
        *outMin = 0;
        *outMax = 0;
        return;
    }
    float lo = v[0];
    float hi = v[0];
    for (int i = 1; i < n; i++) {
        if (v[i] < lo) {
            lo = v[i];
        }
        if (v[i] > hi) {
            hi = v[i];
        }
    }
    *outMin = lo;
    *outMax = hi;
}

static int FirstIndexOf(const float* v, int n, float want) {
    for (int i = 0; i < n; i++) {
        if (v[i] == want) {
            return i;
        }
    }
    return 0;
}

ScaleLinear ScaleLinear::New(const float* domain, int domainN,
                             const float* range, int rangeN) {
    float domainMin = 0, domainMax = 0;
    MinMax(domain, domainN, &domainMin, &domainMax);

    float rangeMin = 0, rangeMax = 0;
    MinMax(range, rangeN, &rangeMin, &rangeMax);
    float rangeFrom = rangeMin;
    float rangeTo = rangeMax;
    if (rangeN > 0) {
        // Whichever of the two comes first is where the range starts, so a
        // descending range keeps mapping the domain backwards. A range with
        // more than two stops is still only its ends.
        if (FirstIndexOf(range, rangeN, rangeMin) >
            FirstIndexOf(range, rangeN, rangeMax)) {
            rangeFrom = rangeMax;
            rangeTo = rangeMin;
        }
    }

    ScaleLinear s;
    s.domainLen = domainN;
    s.domainStart = domainMin;
    s.domainDiff = domainMax - domainMin;
    s.rangeStart = rangeFrom;
    s.rangeDiff = rangeTo - rangeFrom;
    return s;
}

bool ScaleLinear::Tick(float value, float* out) const {
    if (domainDiff == 0) {
        return false;
    }
    float ratio = (value - domainStart) / domainDiff;
    *out = ratio * rangeDiff + rangeStart;
    return true;
}

void ScaleLinear::LeastIndexWithDomain(float tick, const float* domain,
                                       int domainN, int* outIndex,
                                       float* outTick) const {
    *outIndex = 0;
    *outTick = 0;
    if (domainLen == 0 || domainN <= 0) {
        return;
    }
    // Rust enumerates after dropping the domain values that have no tick, so
    // the index counts the ones that resolved.
    int seen = 0;
    bool any = false;
    float bestDist = 0;
    for (int i = 0; i < domainN; i++) {
        float t = 0;
        if (!Tick(domain[i], &t)) {
            continue;
        }
        float dist = t - tick;
        if (dist < 0) {
            dist = -dist;
        }
        if (!any || dist < bestDist) {
            any = true;
            bestDist = dist;
            *outIndex = seen;
            *outTick = t;
        }
        seen++;
    }
}

ScalePoint ScalePoint::New(const float* domain, int domainN, const float* range,
                           int rangeN) {
    ScalePoint s;
    s.domain = domain;
    s.domainLen = domainN;
    if (domainN == 0) {
        return s;
    }
    float rangeMin = 0, rangeMax = 0;
    MinMax(range, rangeN, &rangeMin, &rangeMax);
    float diff = rangeMax - rangeMin;
    s.rangeStart = rangeMin;
    s.rangeTick = domainN == 1 ? diff : diff / (float)(domainN - 1);
    return s;
}

bool ScalePoint::Tick(float value, float* out) const {
    int index = -1;
    for (int i = 0; i < domainLen; i++) {
        if (domain[i] == value) {
            index = i;
            break;
        }
    }
    if (index < 0) {
        return false;
    }
    // A single point has no spacing to step by, so it sits in the middle.
    *out = domainLen == 1 ? rangeStart + rangeTick * 0.5f
                          : rangeStart + (float)index * rangeTick;
    return true;
}

int ScalePoint::LeastIndex(float tick) const {
    if (domainLen <= 0 || rangeTick == 0) {
        return 0;
    }
    // roundf, not rint: a tick exactly between two points belongs to the
    // later one, which is what Rust's f32::round does and what the ties in
    // the reference tests assert.
    float index = roundf((tick - rangeStart) / rangeTick);
    if (index < 0) {
        return 0;
    }
    if (index > (float)(domainLen - 1)) {
        return domainLen - 1;
    }
    return (int)index;
}

int ScaleOrdinal::Map(int domainIndex) const {
    if (domainIndex < 0) {
        return unknown;
    }
    if (rangeLen <= 0) {
        return -1;
    }
    return domainIndex % rangeLen;
}

} // namespace component
} // namespace gpui
