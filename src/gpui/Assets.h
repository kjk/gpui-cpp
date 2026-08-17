
#include "Base.h"

// Asset roots searched for rust-embed-style paths such as "icons/inbox.svg".

namespace gpui {

void AssetsClear();
void AssetsAddRoot(Str dir);
// Walk cwd and the exe directory looking for assets/<exampleName>.
void AssetsAddDefaultRoots(Str exampleName);
bool AssetsLoad(Str relPath, Vec<u8>* out);
TempStr AssetsLoadTextTemp(Str relPath);
bool AssetsExists(Str relPath);
} // namespace gpui
