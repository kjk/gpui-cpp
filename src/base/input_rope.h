#ifndef GPUI_BASE_INPUT_ROPE_H_
#define GPUI_BASE_INPUT_ROPE_H_
/* Flat-text counterpart of crates/base/src/input/base/rope_ext.rs. */

#include "gpui/gpui.h"

namespace gpui {

// InputEdit moved to gpui/gpui.h when InputState grew the pending-edit
// envelope the highlighter seam consumes.

struct RopeLines {
    Str rope = {};
    int row = 0;
    int endRow = 0;

    static RopeLines New(Str rope);
    bool Next(Str* out);
    int Len() const { return std::max(0, endRow - row); }
};

// A value facade for the source's RopeExt trait. It owns no text and forwards
// to the established UTF-8 byte-offset helpers.
struct RopeExt {
    Str text = {};

    static RopeExt Of(Str text) { return RopeExt{text}; }
    int LineStartOffset(int row) const;
    int LineEndOffset(int row) const;
    Str SliceLine(int row) const;
    Str SliceLines(int firstRow, int endRow) const;
    RopeLines IterLines() const;
    int LinesLen() const;
    int LineLen(int row) const;
    int CharAt(int offset, uint32_t* out) const;
    RopePoint OffsetToPoint(int offset) const;
    int PointToOffset(RopePoint point) const;
    int OffsetUtf16ToOffset(int offset) const;
    int OffsetToOffsetUtf16(int offset) const;
    int ClipOffset(int offset, Bias bias) const;
    bool WordRange(int offset, Selection* out) const;
    Str WordAt(int offset) const;
};

} // namespace gpui
#endif // GPUI_BASE_INPUT_ROPE_H_
