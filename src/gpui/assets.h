#ifndef GPUI_GPUI_ASSETS_H_
#define GPUI_GPUI_ASSETS_H_

#include "base.h"

// Asset roots searched for rust-embed-style paths such as "icons/inbox.svg".

namespace gpui {

void AssetsClear();
// How many roots are registered.
int AssetsRootCount();
void AssetsAddRoot(Str dir);
// Walk cwd and the exe directory looking for assets/<exampleName>.
void AssetsAddDefaultRoots(Str exampleName);
bool AssetsLoad(Str relPath, Vec<uint8_t>* out);
TempStr AssetsLoadTextTemp(Str relPath);
// The first asset root that has `relDir` under it, as a native path. False
// when no root does. AssetsLoad answers for one file; a caller that has to
// list a folder — the theme registry reads a directory of theme files —
// needs the folder itself.
bool AssetsFindDir(Str relDir, char* out, int cap);
bool AssetsExists(Str relPath);
} // namespace gpui
#endif // GPUI_GPUI_ASSETS_H_
