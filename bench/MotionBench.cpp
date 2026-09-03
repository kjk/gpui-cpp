/* Port of crates/base/benches/motion.rs — the motion core's steady sampling
   paths, which are the ones a frame runs a thousand of.

   The Rust bench is not a criterion one: it is a `main` that warms up, times
   31 batches of 200 iterations, and reports the median, the p95 and the
   worst. It also installs a counting global allocator and asserts that the
   steady loop allocated nothing at all, then fails outright if the scalar
   workload's median passes 100 µs.

   Neither half of that survives the trip, and for the same reason in both
   cases. The allocation assertion is what Rust needs to prove that
   `Timing::sample`, `Keyframes::sample` and `Stagger::delay` do not build a
   schedule behind the caller's back; here they cannot — `Keyframes` borrows
   the array it was validated against and every other type is a POD copied by
   value, so there is no allocator to count. The budget is a wall clock on a
   machine we do not control, and `bench/bench.cpp` reports the median and the
   minimum of ten samples rather than failing a run. The workloads themselves
   are the same four, with the same iteration counts and the same inputs, so
   the numbers are comparable to the table in the checkin.

   What each measures:

     timing     Timing::sample plus the easing it carries, over an elapsed
                time that walks the whole active interval. The one Rust puts
                a budget on.
     keyframes  Keyframes::sample at 2, 8 and 32 frames, which is the linear
                scan for the segment plus one interpolation.
     spring     the analytic spring step, which is the closed form rather
                than an integration and so costs the same at any frame time.
     stagger    Stagger::delay, integer arithmetic and no schedule. */

#include "Bench.h"

// The Rust bench's shape: 200 iterations of the workload per sample, so one
// row is the cost of a thousand samples times two hundred.
static const int kIterations = 200;
static const int kSamples = 1000;

// ─── timing + easing ─────────────────────────────────────────────────────

struct TimingCase {
    Timing timing;
    double sum = 0;
};

static void TimingSetup(TimingCase* c) {
    c->sum = 0;
}

static void TimingRun(TimingCase* c) {
    for (int it = 0; it < kIterations; it++) {
        for (int index = 0; index < kSamples; index++) {
            // Rust walks a 240 ms interval with a stride that is coprime with
            // it, so the samples land all over the curve rather than on a few
            // points of it.
            float elapsedMs =
                (float)((int64_t)index * 211003 % 240000000) / 1000000.f;
            c->sum += c->timing.Sample(elapsedMs).directedProgress;
        }
    }
    BenchKeep(&c->sum);
}

// ─── keyframes ───────────────────────────────────────────────────────────

// The largest track the bench builds, so one array serves all three.
static Keyframe<float> gFrames[32];

struct KeyframeCase {
    Keyframes<float> track;
    double sum = 0;
};

static void KeyframeSetup(KeyframeCase* c) {
    c->sum = 0;
}

static void KeyframeRun(KeyframeCase* c) {
    for (int it = 0; it < kIterations; it++) {
        for (int index = 0; index < kSamples; index++) {
            c->sum += c->track.Sample((float)(index % 997) / 996.f);
        }
    }
    BenchKeep(&c->sum);
}

// ─── springs ─────────────────────────────────────────────────────────────

struct SpringCase {
    Spring spring;
    SpringState state;
};

static void SpringSetup(SpringCase* c) {
    c->state = SpringState{};
}

static void SpringRun(SpringCase* c) {
    // Rust steps GPUI's SpringConfig directly, which is one propagation with
    // no keyed state around it. SpringAdvance is that step plus the settle
    // check, driven off a clock the caller owns — the same seam the tests
    // use, and the only way to run the spring without a window.
    for (int it = 0; it < kIterations; it++) {
        SpringState st;
        st.init = true;
        st.position = 0;
        st.velocity = 0;
        st.target = 1.f;
        double now = 0;
        for (int index = 0; index < kSamples; index++) {
            now += (double)(index % 3 + 1) / 240.0;
            SpringAdvance(&st, 1.f, c->spring, now, false);
        }
        c->state = st;
    }
    BenchKeep(&c->state);
}

// ─── stagger ─────────────────────────────────────────────────────────────

struct StaggerCase {
    Stagger stagger = Stagger::New(24.f, StaggerOrigin::CenterOrigin());
    double sum = 0;
};

static void StaggerSetup(StaggerCase* c) {
    c->sum = 0;
}

static void StaggerRun(StaggerCase* c) {
    for (int it = 0; it < kIterations; it++) {
        for (int index = 0; index < kSamples; index++) {
            c->sum += c->stagger.Delay(index % 32, 32);
        }
    }
    BenchKeep(&c->sum);
}

void BenchMotion() {
    const char* group = "motion";

    TimingCase timing;
    timing.timing = Timing::New(240).Ease(Easing::Ease());
    BenchCase(group, "timing + easing samples", "samples",
              (int64_t)kSamples * kIterations, MkFunc0(TimingSetup, &timing),
              MkFunc0(TimingRun, &timing));

    const int frameCounts[3] = {2, 8, 32};
    for (int i = 0; i < 3; i++) {
        int count = frameCounts[i];
        for (int ix = 0; ix < count; ix++) {
            float offset = (float)ix / (float)(count - 1);
            gFrames[ix] = Keyframe<float>::New(offset, offset * 100.f)
                              .Ease(Easing::Ease());
        }
        KeyframeCase kf;
        kf.track = Keyframes<float>::TryNew(gFrames, count).Unwrap();
        const char* name = count == 2   ? "keyframe samples (2 frames)"
                           : count == 8 ? "keyframe samples (8 frames)"
                                        : "keyframe samples (32 frames)";
        BenchCase(group, name, "samples", (int64_t)kSamples * kIterations,
                  MkFunc0(KeyframeSetup, &kf), MkFunc0(KeyframeRun, &kf));
    }

    SpringCase spring;
    // SpringConfig::new(438.65, 41.89, 1.0) is a stiffness and a damping
    // coefficient over unit mass, which is w0 = sqrt(438.65) = 20.94 rad/s —
    // a response of 300 ms — at a damping ratio of 41.89 / (2 * w0) = 1.0.
    spring.spring = SpringNew(300.f);
    BenchCase(group, "analytic spring integration samples", "samples",
              (int64_t)kSamples * kIterations, MkFunc0(SpringSetup, &spring),
              MkFunc0(SpringRun, &spring));

    StaggerCase stagger;
    BenchCase(group, "stagger delay calculations", "samples",
              (int64_t)kSamples * kIterations, MkFunc0(StaggerSetup, &stagger),
              MkFunc0(StaggerRun, &stagger));
}
