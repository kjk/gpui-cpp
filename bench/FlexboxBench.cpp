/* Port of taffy's benches/benches/flexbox.rs: five shapes of large,
   pseudo-randomly generated flexbox tree.

   Rust's node counts are gated by the `small` and `large` cargo features;
   here they are the -small and -large flags, and the middle case always runs.
   The Rust also builds each shape for Yoga and taffy 0.3 behind features, to
   time the three against each other. Only taffy is ported. */

#include "Bench.h"

using namespace taffy;

// ─── the style generators ────────────────────────────────────────────────

// Rust's `FixedStyleGenerator`: one style, handed out for every node.
struct FixedStyleGen {
    taffy::Style style;
};

static taffy::Style FixedStyleFn(BenchRng* rng, void* ud) {
    (void)rng;
    return ((FixedStyleGen*)ud)->style;
}

static RectLpa MarginLength(float v) {
    LengthPercentageAuto l = LengthPercentageAuto::Length(v);
    return {l, l, l, l};
}

static Dimension RandomDimension(BenchRng* rng) {
    float r = rng->Range(0.0f, 1.0f);
    if (r < 0.2f) {
        return Dimension::Auto();
    }
    if (r < 0.8f) {
        return Dimension::Length(rng->Range(0.0f, 500.0f));
    }
    return Dimension::Percent(rng->Range(0.0f, 1.0f));
}

// Rust's `RandomStyleGenerator`.
static taffy::Style RandomStyleFn(BenchRng* rng, void* ud) {
    (void)ud;
    taffy::Style s;
    s.size = {RandomDimension(rng), RandomDimension(rng)};
    return s;
}

// Rust's `SuperDeepStyleGen`. Its commented-out line picks the flex direction
// at random; the live one is always a row, and this follows the live one.
static taffy::Style SuperDeepStyleFn(BenchRng* rng, void* ud) {
    (void)rng;
    (void)ud;
    taffy::Style s;
    s.flexDirection = FlexDirection::Row;
    s.flexGrow = 1.0f;
    s.margin = MarginLength(10.0f);
    return s;
}

// ─── the cases ───────────────────────────────────────────────────────────

// What a benchmark's setup rebuilds and its run lays out. One struct rather
// than one per shape, since the shapes differ only in which builder call the
// setup makes.
struct FlexCase {
    TreeBuilder b;
    StyleGen gen;
    uint32_t nodeCount = 0;
    uint32_t branchingFactor = 0;
    uint32_t depth = 0;
    uint32_t nodesPerLevel = 0;
};

static void DeepSetup(FlexCase* c) {
    c->b.Free();
    c->b.Init(c->gen);
    c->b.BuildDeepHierarchy(c->nodeCount, c->branchingFactor);
}

static void FlatSetup(FlexCase* c) {
    c->b.Free();
    c->b.Init(c->gen);
    c->b.BuildFlatHierarchy(c->nodeCount);
}

static void SuperDeepSetup(FlexCase* c) {
    c->b.Free();
    c->b.Init(c->gen);
    c->b.BuildSuperDeepHierarchy(c->depth, c->nodesPerLevel);
}

// Every flexbox benchmark lays out against `compute_layout(None, None)`,
// which is max-content in both axes.
static void FlexRun(FlexCase* c) {
    c->b.ComputeLayout(Optf(), Optf());
    BenchKeep(&c->b.tree);
}

// Runs one case and drops the tree the last sample left behind.
static void RunFlexCase(const char* group, const char* name, int64_t param,
                        const char* unit, void (*setup)(FlexCase*),
                        FlexCase* c) {
    BenchCase(group, name, unit, param, MkFunc0(setup, c), MkFunc0(FlexRun, c));
    c->b.Free();
}

// ─── huge_nested_benchmarks ──────────────────────────────────────────────

// Rust names the group `yoga 'huge nested'` — the shape came from Yoga's own
// benchmark suite.
static void HugeNested() {
    const char* group = "flexbox/huge nested";
    FixedStyleGen fixed;
    fixed.style.size = SizeDim::FromLengths(10.0f, 10.0f);
    fixed.style.flexGrow = 1.0f;

    uint32_t counts[3] = {0, 10000, 0};
    if (gBenchSmall) {
        counts[0] = 1000;
    }
    if (gBenchLarge) {
        counts[2] = 100000;
    }

    for (int i = 0; i < 3; i++) {
        if (counts[i] == 0) {
            continue;
        }
        FlexCase c;
        c.gen = {FixedStyleFn, FixedStyleFn, nullptr, &fixed};
        c.nodeCount = counts[i];
        c.branchingFactor = 10;
        RunFlexCase(group, "(10-way branching)", counts[i], "nodes", DeepSetup,
                    &c);
    }
}

// ─── wide_benchmarks ─────────────────────────────────────────────────────

static void Wide() {
    const char* group = "flexbox/wide tree";
    uint32_t counts[3] = {0, 10000, 0};
    if (gBenchSmall) {
        counts[0] = 1000;
    }
    if (gBenchLarge) {
        counts[2] = 100000;
    }

    for (int i = 0; i < 3; i++) {
        if (counts[i] == 0) {
            continue;
        }
        FlexCase c;
        c.gen = {RandomStyleFn, RandomStyleFn, nullptr, nullptr};
        c.nodeCount = counts[i];
        RunFlexCase(group, "(2-level hierarchy)", counts[i], "nodes", FlatSetup,
                    &c);
    }
}

// ─── deep_auto_benchmarks and deep_random_benchmarks ─────────────────────

// The two deep benchmarks differ only in the style generator: one hands every
// node the same auto-sized style, the other a random size.
static void Deep(const char* group, const StyleGen& gen) {
    struct Case {
        uint32_t nodeCount;
        const char* label;
        bool largeOnly;
    };
    Case cases[3] = {{4000, "(12-level hierarchy)", false},
                     {10000, "(14-level hierarchy)", false},
                     {100000, "(17-level hierarchy)", true}};

    for (int i = 0; i < 3; i++) {
        if (cases[i].largeOnly && !gBenchLarge) {
            continue;
        }
        FlexCase c;
        c.gen = gen;
        c.nodeCount = cases[i].nodeCount;
        c.branchingFactor = 2;
        RunFlexCase(group, cases[i].label, cases[i].nodeCount, "nodes",
                    DeepSetup, &c);
    }
}

// ─── super_deep_benchmarks ───────────────────────────────────────────────

static void SuperDeep() {
    const char* group = "flexbox/super deep";
    uint32_t depths[3] = {0, 100, 0};
    if (gBenchSmall) {
        depths[0] = 50;
    }
    if (gBenchLarge) {
        depths[2] = 200;
    }

    for (int i = 0; i < 3; i++) {
        if (depths[i] == 0) {
            continue;
        }
        FlexCase c;
        c.gen = {SuperDeepStyleFn, SuperDeepStyleFn, nullptr, nullptr};
        c.depth = depths[i];
        c.nodesPerLevel = 3;
        RunFlexCase(group, "(3 nodes per level)", depths[i], "levels",
                    SuperDeepSetup, &c);
    }
}

void BenchFlexbox() {
    HugeNested();
    Wide();

    FixedStyleGen autoSized;
    autoSized.style.flexGrow = 1.0f;
    autoSized.style.margin = MarginLength(10.0f);
    StyleGen autoGen = {FixedStyleFn, FixedStyleFn, nullptr, &autoSized};
    Deep("flexbox/deep tree (auto size)", autoGen);

    StyleGen randomGen = {RandomStyleFn, RandomStyleFn, nullptr, nullptr};
    Deep("flexbox/deep tree (random size)", randomGen);

    SuperDeep();
}
