/* Themed description list — crates/ui/src/description_list.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

// DescriptionText's tagged payload. Rust's Text and AnyElement variants have
// already become an El* by the time the frame tree is built, but retaining
// the tag preserves its public three-way construction surface.
enum class DescriptionTextKind : uint8_t {
    String,
    Text,
    AnyElement
};

struct DescriptionText {
    DescriptionTextKind kind = DescriptionTextKind::String;
    Str string = {};
    El* element = nullptr;

    static DescriptionText From(Str text);
    static DescriptionText Text(El* text);
    static DescriptionText AnyElement(El* element);
    El* IntoEl(Ctx* cx) const;
};

// One row cell, or DescriptionItem::Separator as its own full-span row.
struct DescriptionItem {
    DescriptionText label = {};
    DescriptionText value = {};
    int span = 1;
    bool separator = false;

    static DescriptionItem New(DescriptionText label);
    static DescriptionItem Separator();
    DescriptionItem& Value(DescriptionText value);
    DescriptionItem& Span(int spanValue);
};

// group_item_rows. `rowCounts` may be null; answers the total number of rows.
// Its capacity is in ints, and at most n + 1 are required (an over-wide first
// item creates the same leading empty row Rust's implementation does).
int DescriptionGroupRows(const DescriptionItem* items, int n, int columns,
                         int* rowCounts = nullptr, int capacity = 0);

struct DescriptionList {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    ArenaVec<DescriptionItem> items;
    int columns = 3;
    float labelWidth = 120;
    bool bordered = true;
    // DescriptionList::layout: Horizontal puts the label beside the value,
    // Vertical stacks it above.
    bool vertical = false;
    UiSize size = UiSize::Medium;

    static DescriptionList* New(Ctx* cx);
    static DescriptionList* Horizontal(Ctx* cx);
    static DescriptionList* Vertical(Ctx* cx);
    // The value is text; Value() takes a built element instead.
    DescriptionList* Item(Str label, Str value, int span = 1);
    DescriptionList* Item(DescriptionText label, DescriptionText value,
                          int span = 1);
    DescriptionList* ItemEl(Str label, El* value, int span = 1);
    DescriptionList* Child(const DescriptionItem& item);
    DescriptionList* Separator();
    DescriptionList* Columns(int n);
    DescriptionList* LabelWidth(float w);
    DescriptionList* Bordered(bool v);
    DescriptionList* Layout(Axis axis);
    DescriptionList* Vertical(bool v = true);
    DescriptionList* WithSize(UiSize s);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
