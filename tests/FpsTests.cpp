/* Ported from crates/fps/src/sampler.rs, mod tests.
 *
 * `ingests_frames_from_other_windows` has no counterpart: Rust filters a
 * process-wide frame trace by window id, while ours is already per-window, so
 * there is nothing to filter. The rest is the same arithmetic on the same
 * rolling window. */

#include "Test.h"

#include <math.h>

static void Ingest(FrameSampler* s, float drawSecs, double at) {
    FrameSamplerIngest(s, &drawSecs, 1, at);
}

static void DropsOldestSamplesBeyondCapacity() {
    FrameSampler s;
    FrameSamplerSetCapacity(&s, 2);

    for (int i = 0; i < 3; i++) {
        Ingest(&s, 0.005f + 0.001f * (float)i, 0);
    }

    utassert(s.n == 2);
    utassertnear(s.draws[0], 0.006f);
    utassertnear(s.draws[1], 0.007f);
}

// Feeds `count` frames spaced `interval` apart and returns the rate.
static float MeasureFps(int count, double interval) {
    FrameSampler s;
    FrameSamplerSetCapacity(&s, 120);
    for (int i = 0; i < count; i++) {
        Ingest(&s, 0.001f, interval * (double)i);
    }
    return FrameSamplerFps(&s);
}

static void FpsIsFramesDividedByTheSpanTheyCover() {
    // The rate is (n - 1) / span, not n / span: n frames delimit n - 1
    // intervals. Counting the frames would over-report by 1 / span, a whole
    // frame per second at these rates.
    //
    // 11 frames 10ms apart cover 100ms => 10 intervals => 100 fps.
    utassert(fabsf(MeasureFps(11, 0.010) - 100.f) < 0.5f);
    // The same span sampled more finely reports the same rate.
    utassert(fabsf(MeasureFps(101, 0.001) - 1000.f) < 5.f);
}

static void FpsMatchesTheCommonRefreshRates() {
    const double intervals[] = {16667e-6, 8333e-6, 33333e-6, 6944e-6};
    const float expected[] = {60.f, 120.f, 30.f, 144.f};
    for (int i = 0; i < 4; i++) {
        // A full second of frames at that interval.
        int count = (int)(1.0 / intervals[i]);
        utassert(fabsf(MeasureFps(count, intervals[i]) - expected[i]) < 1.f);
    }
}

static void FpsNeedsTwoFramesToHaveARateAtAll() {
    // One frame delimits no interval, so there is nothing to divide by and the
    // honest answer is zero rather than a guess.
    utassertnear(MeasureFps(0, 0.010), 0.f);
    utassertnear(MeasureFps(1, 0.010), 0.f);
    utassert(MeasureFps(2, 0.010) > 0.f);
}

static void SimultaneousFramesDoNotDivideByZero() {
    FrameSampler s;
    FrameSamplerSetCapacity(&s, 64);

    // A first tick can drain several frames at once, stamping them all with
    // the same arrival time; the span between them is zero.
    const float draws[] = {0.004f, 0.004f, 0.004f};
    FrameSamplerIngest(&s, draws, 3, 0);

    utassertnear(FrameSamplerFps(&s), 0.f);
}

static void FramesOutsideTheRollingWindowStopCounting() {
    FrameSampler s;
    FrameSamplerSetCapacity(&s, 64);

    for (int i = 0; i < 10; i++) {
        Ingest(&s, 0.004f, 0.010 * (double)i);
    }
    utassert(FrameSamplerFps(&s) > 0.f);

    // Two seconds later the window has gone idle: every retained frame is
    // older than the rolling window, so the rate collapses to zero.
    FrameSamplerIngest(&s, nullptr, 0, 2.0);
    utassertnear(FrameSamplerFps(&s), 0.f);
    // The chart history survives, so the last known shape stays on screen.
    utassert(s.n == 10);
}

static void MeanAndPeakAndOverBudget() {
    FrameSampler s;
    FrameSamplerSetCapacity(&s, 64);
    utassertnear(FrameSamplerMeanDraw(&s), 0.f);
    utassertnear(FrameSamplerPeakDraw(&s), 0.f);
    utassertnear(FrameSamplerOverBudget(&s, 0.016f), 0.f);

    const float draws[] = {0.004f, 0.008f, 0.030f, 0.002f};
    FrameSamplerIngest(&s, draws, 4, 0);
    utassertnear(FrameSamplerMeanDraw(&s), 0.011f);
    utassertnear(FrameSamplerPeakDraw(&s), 0.030f);
    utassertnear(FrameSamplerOverBudget(&s, 0.016f), 0.25f);
}

void TestFrameSampler() {
    TestSuite("fps/sampler");
    DropsOldestSamplesBeyondCapacity();
    FpsIsFramesDividedByTheSpanTheyCover();
    FpsMatchesTheCommonRefreshRates();
    FpsNeedsTwoFramesToHaveARateAtAll();
    SimultaneousFramesDoNotDivideByZero();
    FramesOutsideTheRollingWindowStopCounting();
    MeanAndPeakAndOverBudget();
}
