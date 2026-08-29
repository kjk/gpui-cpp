#ifndef GPUI_BASE_SANKEY_H_
#define GPUI_BASE_SANKEY_H_
/* Sankey layout — crates/ui/src/plot/shape/sankey.rs, after d3-sankey.

   A sankey is a set of nodes in columns and ribbons between them, each ribbon
   as thick as the flow it carries. The layout works out which column every
   node belongs in, how tall it is, where in its column it sits after a few
   relaxation passes, and where each ribbon meets each end.

   Rust keeps a Vec of link indices on every node; the lists here are one
   array with a slice per node, so the graph is four allocations however many
   nodes it has — and the topology of a fifty-thousand-node chain is still one
   pass over each. */

#include "gpui/gpui.h"

namespace gpui {

// Where the columns line up. Rust's SankeyAlign, which is d3's sankeyLeft /
// sankeyRight / sankeyCenter / sankeyJustify.
enum class SankeyAlign : uint8_t {
    Left,
    Right,
    Center,
    Justify
};

// How a flow value becomes a height. Sqrt compresses a wide range so one
// dominant flow does not dwarf the rest, without the caller transforming the
// data — the labels still read the raw values.
enum class SankeyValueScale : uint8_t {
    Linear,
    Sqrt
};

// Why a layout could not be worked out.
enum class SankeyError : uint8_t {
    None,
    // A link names a node that is not there; `errNode` says which.
    MissingNode,
    CircularLink
};

// An input link: `source` and `target` are indices into the node list, which
// is d3-sankey's default nodeId.
struct SankeyLink {
    int source = 0;
    int target = 0;
    double value = 0;
};

// A node with its layout worked out — d3-sankey's computed node fields.
struct SankeyNodeLayout {
    int index = 0;
    // The throughput in the layout's value space: the larger of what comes in
    // and what goes out. Under a non-linear scale this is in scaled units, so
    // a label must not read it.
    double value = 0;
    // The longest path from any source, and to any sink.
    int depth = 0;
    int height = 0;
    // The column, after the alignment has had its say.
    int layer = 0;
    float x0 = 0, x1 = 0, y0 = 0, y1 = 0;
    // The node's slice of the graph's outgoing and incoming link lists.
    int srcStart = 0, srcCount = 0;
    int tgtStart = 0, tgtCount = 0;
};

// A link with its layout worked out. `y0` and `y1` are the centres of the
// ribbon at each end, and each end has its own width: the links on one side
// of a node share that node's height in proportion to their values, so both
// sides of every node are covered. On a balanced graph the two ends match; on
// an imbalanced one the ribbon widens or narrows across.
struct SankeyLinkLayout {
    int index = 0;
    int source = 0;
    int target = 0;
    double value = 0;
    float y0 = 0, y1 = 0;
    // The nominal width from the one global scale, which is what the
    // relaxation works with; it equals both ends on a balanced graph.
    float width = 0;
    float sourceWidth = 0;
    float targetWidth = 0;
};

// The graph, with the link lists as slices into one array each.
struct SankeyGraph {
    Vec<SankeyNodeLayout> nodes;
    Vec<SankeyLinkLayout> links;
    // Link indices, grouped by node — nodes name their slice of each.
    Vec<int> srcLinks;
    Vec<int> tgtLinks;
    // MissingNode names the node a link asked for and did not find.
    int errNode = 0;

    void Reset() {
        VecReset(nodes);
        VecReset(links);
        VecReset(srcLinks);
        VecReset(tgtLinks);
    }
};

// The number of columns: the largest layer plus one, and zero for a graph
// with nothing in it.
int SankeyLayerCount(const SankeyGraph* g);

// The layout generator, with d3-sankey's defaults.
struct Sankey {
    float nodeWidth = 24.f;
    float nodePadding = 8.f;
    SankeyAlign align = SankeyAlign::Justify;
    int iterations = 6;
    SankeyValueScale valueScale = SankeyValueScale::Linear;
    // The extent, [[x0, y0], [x1, y1]].
    float x0 = 0, y0 = 0, x1 = 1, y1 = 1;
};

// The topology alone: each node's value, depth, height, layer and horizontal
// place, without any of the vertical placement. Much cheaper than the whole
// layout, which is what lets a chart measure its labels against the column
// structure before it knows the extent.
SankeyError SankeyTopology(const Sankey* s, int nodeCount,
                           const SankeyLink* links, int nLinks,
                           SankeyGraph* out);
// The rest of the layout for a graph the topology built: the vertical
// placement and the ribbon ends, on this generator's extent. The topological
// fields do not depend on the extent, which is why the second pass can reuse
// them.
void SankeyLayoutFrom(const Sankey* s, SankeyGraph* g);
// Both passes at once.
SankeyError SankeyLayout(const Sankey* s, int nodeCount,
                         const SankeyLink* links, int nLinks, SankeyGraph* out);

} // namespace gpui
#endif // GPUI_BASE_SANKEY_H_
