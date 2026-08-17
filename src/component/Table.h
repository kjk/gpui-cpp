/* Themed table — crates/ui/src/table */

#include "component/Common.h"

namespace gpui {

namespace component {

struct Table {
    Arena* a = nullptr;
    const char** heads = nullptr;
    int nHeads = 0;
    const char*** rows = nullptr;
    int nRows = 0;

    static Table* New(Arena* a);
    Table* Heads(const char** h, int n);
    Table* Rows(const char*** r, int n);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
