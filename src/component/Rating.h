/* Themed rating — crates/ui/src/rating.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

struct Rating {
    Arena* a = nullptr;
    int value = 0;
    int max = 5;
    Func1<int> onChange;

    static Rating* New(Arena* a);
    Rating* Value(int v);
    Rating* Max(int v);
    Rating* OnChange(Func1<int> fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
