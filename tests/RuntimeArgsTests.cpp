/* The platform entry point consumes runtime-owned arguments before GpuiMain.
 * Keep that filtering separate from application arguments: story, shell and
 * user programs all parse argv themselves. */

#include "Test.h"

void TestRuntimeArgs() {
    TestSuite("runtime args");
#if GPUI_OS_WINDOWS
#if WIN_BACKEND_D3D12
    char paint[] = "__paint=d3d12";
    WinPaintBackend expected = WinPaintBackend::D3D12;
#elif WIN_BACKEND_D3D11
    char paint[] = "__paint=d3d11";
    WinPaintBackend expected = WinPaintBackend::D3D11;
#else
    char paint[] = "__paint=d2d";
    WinPaintBackend expected = WinPaintBackend::Direct2D;
#endif
    char app[] = "tests";
    char first[] = "first";
    char msaa[] = "__msaa=8";
    char scene[] = "__scene=damage";
    char second[] = "second";
    char* argv[] = {app, first, paint, msaa, scene, second, nullptr};

    int argc = GpuiTakeRuntimeArgs(6, argv);
    const WinPaintOptions& options = WinPaintOptionsGet();
    utassert(argc == 3);
    utassert(base::StrEq(Str(argv[0]), "tests"));
    utassert(base::StrEq(Str(argv[1]), "first"));
    utassert(base::StrEq(Str(argv[2]), "second"));
    utassert(argv[3] == nullptr);
    utassert(options.backend == expected);
    utassert(options.msaa == WinPaintMsaa::X8);
    utassert(options.scene == WinSceneMode::Damage);
#else
    utassert(true);
#endif
}
