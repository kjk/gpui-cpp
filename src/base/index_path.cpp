#include "base/index_path.h"

namespace gpui {

Str IndexPathIdStr(Arena* a, IndexPath p) {
    return StrDup(a, fmt("index-path(%d,%d,%d)", p.section, p.row, p.column));
}

uint32_t IndexPathClickId(IndexPath p) {
    return HashClickId(fmt("index-path(%d,%d,%d)", p.section, p.row, p.column));
}

} // namespace gpui
