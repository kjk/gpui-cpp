/* Themed table — crates/ui/src/table */

#include "ui/sizing.h"

namespace gpui {

namespace component {

struct Table {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    const char** heads = nullptr;
    int nHeads = 0;
    const char*** rows = nullptr;
    int nRows = 0;

    static Table* New(Ctx* cx);
    Table* Heads(const char** h, int n);
    Table* Rows(const char*** r, int n);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
