#include "shell/filesystem.h"

namespace gpui::shell {

void FsResult::Free() {
    StrFree(bytes);
    bytes = {};
    for (int i = 0; i < entries.len; i++) StrFree(entries[i].name);
    entries.Reset();
    exists = false;
}

} // namespace gpui::shell
