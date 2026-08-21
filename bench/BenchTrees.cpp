/* Port of taffy's benches/src/lib.rs: the random source and the tree shapes
   the benchmarks are built from.

   Rust states these as the `BuildTree` and `BuildTreeExt` traits so that one
   benchmark can build the same shape for taffy, for taffy 0.3 and for Yoga
   and time all three. There is one tree here, so the traits collapse into
   `TreeBuilder`, and the method names are the trait method names. */

#include "Bench.h"

using namespace taffy;

const uint64_t kStandardRngSeed = 12345;

// ─── BenchRng ────────────────────────────────────────────────────────────

// PCG32, in the form its author publishes it. See the note in Bench.h for
// why this rather than the ChaCha8 stream Rust draws from.
void BenchRng::Seed(uint64_t seed) {
    state = 0;
    inc = (seed << 1) | 1;
    NextU32();
    state += seed;
    NextU32();
}

uint32_t BenchRng::NextU32() {
    uint64_t old = state;
    state = old * 6364136223846793005ULL + inc;
    uint32_t xorshifted = (uint32_t)(((old >> 18) ^ old) >> 27);
    uint32_t rot = (uint32_t)(old >> 59);
    return (xorshifted >> rot) | (xorshifted << ((0u - rot) & 31));
}

float BenchRng::NextFloat() {
    // 24 bits is every value a float can hold in [0, 1) without rounding.
    return (float)(NextU32() >> 8) * (1.0f / 16777216.0f);
}

float BenchRng::Range(float lo, float hi) {
    return lo + (hi - lo) * NextFloat();
}

int BenchRng::RangeInt(int lo, int hi) {
    uint32_t span = (uint32_t)(hi - lo) + 1;
    return lo + (int)(NextU32() % span);
}

// ─── TreeBuilder ─────────────────────────────────────────────────────────

void TreeBuilder::Init(const StyleGen& styleGen, int capacity) {
    tree.Init(capacity);
    rng.Seed(kStandardRngSeed);
    gen = styleGen;
    taffy::Style rootStyle;
    if (gen.root) {
        rootStyle = gen.root(&rng, gen.ud);
    }
    root = tree.NewLeaf(rootStyle);
}

void TreeBuilder::Free() {
    tree.Free();
}

NodeId TreeBuilder::CreateLeafNode() {
    return tree.NewLeaf(gen.leaf(&rng, gen.ud));
}

NodeId TreeBuilder::CreateContainerNode(const NodeId* children, int n) {
    return tree.NewWithChildren(gen.container(&rng, gen.ud), children, n);
}

void TreeBuilder::SetRootChildren(const NodeId* children, int n) {
    tree.SetChildren(root, children, n);
}

void TreeBuilder::BuildDeepTree(uint32_t maxNodes, uint32_t branchingFactor,
                                Vec<NodeId>* out) {
    if (maxNodes <= branchingFactor) {
        for (uint32_t i = 0; i < maxNodes; i++) {
            out->Append(CreateLeafNode());
        }
        return;
    }

    // Another layer, with the remaining nodes split evenly between the
    // children of this one.
    for (uint32_t i = 0; i < branchingFactor; i++) {
        uint32_t sub = (maxNodes - branchingFactor) / branchingFactor;
        Vec<NodeId> children;
        BuildDeepTree(sub, branchingFactor, &children);
        out->Append(CreateContainerNode(children.els, children.len));
    }
}

void TreeBuilder::BuildDeepHierarchy(uint32_t nodeCount,
                                     uint32_t branchingFactor) {
    Vec<NodeId> children;
    BuildDeepTree(nodeCount, branchingFactor, &children);
    SetRootChildren(children.els, children.len);
}

void TreeBuilder::BuildFlatHierarchy(uint32_t targetNodeCount) {
    Vec<NodeId> children;
    while ((uint32_t)TotalNodeCount() < targetNodeCount) {
        int count = rng.RangeInt(1, 4);
        Vec<NodeId> sub;
        for (int i = 0; i < count; i++) {
            sub.Append(CreateLeafNode());
        }
        children.Append(CreateContainerNode(sub.els, sub.len));
    }
    SetRootChildren(children.els, children.len);
}

void TreeBuilder::BuildSuperDeepHierarchy(uint32_t depth,
                                          uint32_t nodesPerLevel) {
    Vec<NodeId> children;
    for (uint32_t i = 0; i < depth; i++) {
        NodeId nodeWithChildren =
            CreateContainerNode(children.els, children.len);
        children.len = 0;
        children.Append(nodeWithChildren);
        for (uint32_t j = 0; j + 1 < nodesPerLevel; j++) {
            children.Append(CreateLeafNode());
        }
    }
    SetRootChildren(children.els, children.len);
}

void TreeBuilder::ComputeLayout(Optf availableWidth, Optf availableHeight) {
    SizeOptF avail;
    avail.width = availableWidth;
    avail.height = availableHeight;
    tree.ComputeLayout(root, SizeAvail::From(avail));
}
