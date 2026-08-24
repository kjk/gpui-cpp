/* Word and line boundaries — crates/base/src/text_boundary.rs
 *
 * What a double click takes, and what a triple click takes. `CharKind`
 * itself is in `gpui/gpui.h`: Rust keeps a `CharacterKind` private to
 * this module and the input engine keeps another, and this tree has the
 * one both walk the text with. `Utf8At` and `Utf8Prev` stay there too —
 * they are the `char` iteration Rust gets from `str`, not part of this
 * module. */

#include "gpui/gpui.h"

namespace gpui {

// CharacterKind::from: which of the four classes a character is in.
CharKind CharKindOf(uint32_t c);
// clip_offset_left: into the string, then back to a character boundary.
int Utf8ClipLeft(Str s, int off);

// word_range_at: the byte range of the word around `off`
// — a run of word characters, or the run of spaces when the offset is in
// one, or the single character otherwise. False when there is nothing there.
bool TextWordRangeAt(Str s, int off, int* outA, int* outB);
// The same for the line: back to the previous newline, on to the next.
void TextLineRangeAt(Str s, int off, int* outA, int* outB);

} // namespace gpui
