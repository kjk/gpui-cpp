#include "base.h"

// Every file in `assets/icons`, converted to the `drawops.h` byte stream
// ahead of time by `cmd/svg-to-bytecode.ts` and compiled in. One array holds
// all of them end to end; an icon is a slice of it.
//
//   bun cmd/svg-to-bytecode.ts
//
// The data lives in `asset_icons.cpp`, which is generated — do not edit it.
// The two lookups below are in `svg.cpp`, next to the runtime `.svg` reader
// they stand in front of.

namespace gpui {

// Where one icon sits in kAssetIconsData. `name` is the file's base name
// without the extension: "chevrons-up-down".
struct AssetIcon {
    const char* name;
    int offset;
    int len;
};

extern const uint8_t kAssetIconsData[];
extern const int kAssetIconsDataLen;
// Sorted by name, so the lookup is a binary search.
extern const AssetIcon kAssetIcons[];
extern const int kAssetIconsCount;

// The bytecode for a base name, or null.
const uint8_t* AssetIconFind(Str name, int* lenOut);
// The same, for a rust-embed-style asset path: "icons/<name>.svg" and nothing
// else — an application's own `.svg` is still read and converted at runtime.
const uint8_t* AssetIconForPath(Str assetPath, int* lenOut);

} // namespace gpui
