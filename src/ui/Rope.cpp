/* Port of crates/base/src/input/base/rope_ext.rs.

   Rust implements `RopeExt` for `ropey::Rope`, whose own API is char-indexed;
   every method there converts to and from byte offsets around a char index.
   The document here is a flat UTF-8 `Str`, so a byte offset is the native
   unit and the conversions run the other way — `char_index_to_offset` and
   `offset_to_char_index` are the two that still have to walk.

   Lines are split on LF alone (`LineType::LF`), so a CRLF document keeps the
   CR at the end of the line: `slice_line` on "World\r\n" is "World\r", and
   `line_end_offset` points at the LF. `word_range` and `word_at` are not
   here — they belong to the language-server hover path, and the word range a
   double click uses is text_boundary.rs's, which is TextWordRangeAt. */

#include "gpui/Gpui.h"

namespace gpui {

int RopeClipOffset(Str text, int offset, Bias bias) {
    if (offset <= 0 || !text.s) {
        return 0;
    }
    if (offset >= text.len) {
        return text.len;
    }
    if (bias == Bias::Left) {
        return Utf8ClipLeft(text, offset);
    }
    // Bias::Right: forward to the next boundary instead.
    while (offset < text.len && ((uint8_t)text.s[offset] & 0xC0) == 0x80) {
        offset++;
    }
    return offset;
}

int RopeCharAt(Str text, int offset, uint32_t* out) {
    *out = 0;
    if (!text.s || offset < 0 || offset >= text.len) {
        return 0;
    }
    return Utf8At(text, offset, out);
}

int RopeLinesLen(Str text) {
    // len_lines(LineType::LF): one more than the number of LFs, and an empty
    // rope still has one line.
    int n = 1;
    for (int i = 0; i < text.len; i++) {
        if (text.s[i] == '\n') {
            n++;
        }
    }
    return n;
}

int RopeLineStartOffset(Str text, int row) {
    // point_to_offset(Point::new(row, 0)): a row past the end is the end.
    if (row <= 0) {
        return 0;
    }
    int seen = 0;
    for (int i = 0; i < text.len; i++) {
        if (text.s[i] != '\n') {
            continue;
        }
        seen++;
        if (seen == row) {
            return i + 1;
        }
    }
    return text.len;
}

Str RopeSliceLine(Str text, int row) {
    if (row < 0 || row >= RopeLinesLen(text)) {
        return {};
    }
    int a = RopeLineStartOffset(text, row);
    int b = a;
    while (b < text.len && text.s[b] != '\n') {
        b++;
    }
    return Str(text.s + a, b - a);
}

int RopeLineLen(Str text, int row) {
    return RopeSliceLine(text, row).len;
}

int RopeLineEndOffset(Str text, int row) {
    return RopeLineStartOffset(text, row) + RopeLineLen(text, row);
}

RopePoint RopeOffsetToPoint(Str text, int offset) {
    offset = RopeClipOffset(text, offset, Bias::Left);
    RopePoint p = {};
    int lineStart = 0;
    for (int i = 0; i < offset; i++) {
        if (text.s[i] == '\n') {
            p.row++;
            lineStart = i + 1;
        }
    }
    p.column = offset - lineStart;
    return p;
}

int RopePointToOffset(Str text, RopePoint point) {
    // Rust does not clamp the column: the callers hand it one they measured
    // off a line, and a column past the end is their bug, not this one's.
    if (point.row < 0 || point.row >= RopeLinesLen(text)) {
        return text.len;
    }
    return RopeLineStartOffset(text, point.row) + point.column;
}

// The two UTF-16 conversions the IME and every `*_utf16` range in state.rs go
// through. A character outside the BMP is one UTF-16 surrogate pair, so it
// counts as two.
int RopeOffsetToOffsetUtf16(Str text, int offset) {
    if (offset > text.len) {
        offset = text.len;
    }
    int n = 0;
    int i = 0;
    while (i < offset) {
        uint32_t c = 0;
        i += Utf8At(text, i, &c);
        n += c >= 0x10000 ? 2 : 1;
    }
    return n;
}

int RopeOffsetUtf16ToOffset(Str text, int offsetUtf16) {
    int n = 0;
    int i = 0;
    while (i < text.len && n < offsetUtf16) {
        uint32_t c = 0;
        int len = Utf8At(text, i, &c);
        n += c >= 0x10000 ? 2 : 1;
        i += len;
    }
    return i;
}

int RopeCharIndexToOffset(Str text, int charIndex) {
    int i = 0;
    int n = 0;
    while (i < text.len && n < charIndex) {
        uint32_t c = 0;
        i += Utf8At(text, i, &c);
        n++;
    }
    return i;
}

int RopeOffsetToCharIndex(Str text, int offset) {
    // Clips right, so an offset landing inside a character counts that whole
    // character.
    offset = RopeClipOffset(text, offset, Bias::Right);
    int i = 0;
    int n = 0;
    while (i < offset) {
        uint32_t c = 0;
        i += Utf8At(text, i, &c);
        n++;
    }
    return n;
}

} // namespace gpui
