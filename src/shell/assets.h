#ifndef GPUI_SHELL_ASSETS_H_
#define GPUI_SHELL_ASSETS_H_
/* Assets shipped in one shell application — crates/shell/src/assets.rs. */

#include "gpui/assets.h"
#include "shell/filesystem.h"

namespace gpui {

constexpr int kShellMaxAssetBytes = 16 * 1024 * 1024;
constexpr int kShellMaxReportedMissingAssets = 256;

struct AppAssets {
    Str root;
    Vec<Str> missing;
    int source = 0;

    explicit AppAssets(Str root);
    AppAssets(const AppAssets&) = delete;
    AppAssets& operator=(const AppAssets&) = delete;
    ~AppAssets();

    bool Install();
    void Uninstall();
    bool Load(Str path, Vec<uint8_t>* out, Str* error = nullptr);
    bool Exists(Str path);
    bool List(Str path, Vec<Str>* out, Str* error = nullptr);
    bool Resolve(Str path, Str* relative, Str* error = nullptr) const;
};

} // namespace gpui
#endif // GPUI_SHELL_ASSETS_H_
