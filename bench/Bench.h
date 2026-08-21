/* The benchmark harness, and the tree builders the benchmarks share.

   Ports of the benchmarks in taffy's `benches/` directory, one file per Rust
   one. That directory is a crate of its own — `taffy_benchmarks` — and is not
   part of the published crate, so it comes from the git checkout the recipe
   in port-upstream.md clones.

   The Rust runs on criterion, which warms up, estimates a sample count and
   reports a confidence interval. This runs a fixed number of samples and
   reports the median and the minimum, which is what a layout number is read
   for: the median says what a frame costs and the minimum says what it costs
   with the noise taken out.

   `benches/mixed.rs` is not ported. It measures text leaves through `parley`,
   a Rust text stack we have no equivalent of; the shape it lays out —
   alternating flex and grid containers — would need our own text measure
   substituted, which makes it a different benchmark rather than a port of
   that one. Worth doing on its own, as `LayoutMeasure` is what every real
   window spends its layout time in. */

#ifndef GPUI_BENCH_H_
#define GPUI_BENCH_H_

#include "gpui.h"

using namespace gpui;

// ─── the harness ─────────────────────────────────────────────────────────

// Samples per benchmark. Criterion picks this per group; here it is one
// number for the whole run, settable with -n=<count>.
extern int gBenchSamples;
// The crate's `small` and `large` cargo features, which gate the smallest and
// largest node counts in `flexbox.rs`.
extern bool gBenchSmall;
extern bool gBenchLarge;
// Only benchmarks whose group or name contains this run. Null means all.
extern const char* gBenchFilter;

/* Runs one benchmark and prints its row.

   `setup` builds a fresh tree and is not timed; `run` is what the clock sees.
   That is criterion's `iter_batched(setup, routine, SmallInput)`, which the
   Rust uses everywhere except `tree_creation.rs`, where building the tree is
   the thing being measured and `setup` does nothing.

   `param` is the number the row is indexed by — the node count, the track
   count, the depth — and `unit` names it. */
void BenchCase(const char* group, const char* name, const char* unit,
               int64_t param, Func0 setup, Func0 run);

// True if a case with this group and name would run. A benchmark whose setup
// is expensive to even reach can ask first.
bool BenchWanted(const char* group, const char* name);

// Rust's `std::hint::black_box`: keeps the compiler from concluding that the
// work had no effect and removing it.
void BenchKeep(const void* p);

// ─── benches/src/lib.rs ──────────────────────────────────────────────────

/* The random source.

   Rust seeds a ChaCha8 stream with `STANDARD_RNG_SEED` so a given benchmark
   always builds the same tree. Reproducing its numbers exactly would mean
   porting ChaCha8 *and* `rand`'s uniform sampling, and the trees would still
   only be comparable against a Rust run on the same machine. What matters
   here is that a tree is the same from one run of this binary to the next, so
   this is a PCG32 carrying the same seed. The shapes are Rust's shapes; the
   values drawn to fill them are not the same values. */
struct BenchRng {
    uint64_t state = 0;
    uint64_t inc = 0;

    void Seed(uint64_t seed);
    uint32_t NextU32();
    // [0, 1)
    float NextFloat();
    // [lo, hi), Rust's `random_range(lo..hi)`.
    float Range(float lo, float hi);
    // [lo, hi], Rust's `random_range(lo..=hi)`.
    int RangeInt(int lo, int hi);
};

extern const uint64_t kStandardRngSeed;

/* How a benchmark makes the styles for the tree it builds.

   Rust's `GenStyle` trait, with `FixedStyleGenerator` and the per-benchmark
   generators as implementations. A function pointer and a `void*` are the
   same thing without the vtable; `root` may be null, which is Rust's default
   method returning `Style::default()`. */
using BenchStyleFn = taffy::Style (*)(BenchRng* rng, void* ud);

struct StyleGen {
    BenchStyleFn leaf = nullptr;
    BenchStyleFn container = nullptr;
    BenchStyleFn root = nullptr;
    void* ud = nullptr;
};

/* Rust's `BuildTree` / `BuildTreeExt`.

   Those are traits so that the same shapes can be built for taffy, taffy 0.3
   and Yoga and timed against each other. There is one tree here, so they
   collapse into it. */
struct TreeBuilder {
    taffy::TaffyTree tree;
    BenchRng rng;
    StyleGen gen;
    taffy::NodeId root;

    // `capacity` is the tree's initial slot count — Rust's
    // `TaffyTree::with_capacity`, whose default is `TaffyTree::new`.
    void Init(const StyleGen& gen, int capacity = 16);
    void Free();

    taffy::NodeId CreateLeafNode();
    taffy::NodeId CreateContainerNode(const taffy::NodeId* children, int n);
    void SetRootChildren(const taffy::NodeId* children, int n);
    int TotalNodeCount() const { return tree.TotalNodeCount(); }

    // A tree `branchingFactor` wide at every level, deep enough to hold
    // `maxNodes`. Appends the nodes it made to `out`.
    void BuildDeepTree(uint32_t maxNodes, uint32_t branchingFactor,
                       Vec<taffy::NodeId>* out);
    void BuildDeepHierarchy(uint32_t nodeCount, uint32_t branchingFactor);
    // Many children, shallow: containers of 1-4 leaves under the root.
    void BuildFlatHierarchy(uint32_t targetNodeCount);
    // One container per level, each with `nodesPerLevel - 1` leaves beside it.
    void BuildSuperDeepHierarchy(uint32_t depth, uint32_t nodesPerLevel);

    void ComputeLayout(taffy::Optf availableWidth, taffy::Optf availableHeight);
};

// The benchmark files.
void BenchFlexbox();
void BenchGrid();
void BenchTreeCreation();

#endif // GPUI_BENCH_H_
