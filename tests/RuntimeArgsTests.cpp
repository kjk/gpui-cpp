/* The platform entry point consumes runtime-owned arguments before GpuiMain.
 * Keep that filtering separate from application arguments: story, shell and
 * user programs all parse argv themselves. */

#include "Test.h"

void TestRuntimeArgs() {
    TestSuite("runtime args");
    char app[] = "tests";
    char first[] = "first";
    char reuse[] = "__layout_reuse=off";
    char second[] = "second";
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
    char msaa[] = "__msaa=8";
    char scene[] = "__scene=damage";
    char reset[] = "__gpu_reset_every=7";
    char* argv[] = {app,   first, paint,  msaa,   scene,
                    reset, reuse, second, nullptr};

    int argc = GpuiTakeRuntimeArgs(8, argv);
    const WinPaintOptions& options = WinPaintOptionsGet();
    utassert(argc == 3);
    utassert(base::StrEq(Str(argv[0]), StrL("tests")));
    utassert(base::StrEq(Str(argv[1]), StrL("first")));
    utassert(base::StrEq(Str(argv[2]), StrL("second")));
    utassert(argv[3] == nullptr);
    utassert(options.backend == expected);
    utassert(options.msaa == WinPaintMsaa::X8);
    utassert(options.scene == WinSceneMode::Damage);
    utassert(options.gpuResetEvery == 7);
#else
    char* argv[] = {app, first, reuse, second, nullptr};
    int argc = GpuiTakeRuntimeArgs(4, argv);
    utassert(argc == 3);
    utassert(base::StrEq(Str(argv[0]), StrL("tests")));
    utassert(base::StrEq(Str(argv[1]), StrL("first")));
    utassert(base::StrEq(Str(argv[2]), StrL("second")));
    utassert(argv[3] == nullptr);
#endif
    utassert(!LayoutReuseOn());

    // Later layout-reuse tests need the cache on; argv is how this process
    // set it, so argv is how it is put back.
    char on[] = "__layout_reuse=on";
    char noReset[] = "__gpu_reset_every=0";
    char* restore[] = {app, on, noReset, nullptr};
    GpuiTakeRuntimeArgs(3, restore);
    utassert(LayoutReuseOn());
}
