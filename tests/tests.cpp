/* The test runner. Builds like an example — it implements GpuiMain and links
   the same amalgam — but the Windows link gives it a console subsystem, so
   printf reaches the terminal that started it.

   Every test here is a port of one in .work/gpui-component at the SHA in
   cmd/versions.ts. Each file says which Rust module it came from. */

#include "Test.h"

#include <math.h>
#include <stdio.h>

int gTestChecks = 0;
int gTestFailures = 0;
const char* gTestSuite = "";

void TestSuite(const char* name) {
    gTestSuite = name;
}

void TestFailed(const char* cond, const char* file, int line) {
    gTestFailures++;
    printf("FAIL %s: %s(%d): %s\n", gTestSuite, file, line, cond);
}

bool TestNear(float a, float b) {
    return fabsf(a - b) < 0.001f;
}

int GpuiMain(int argc, char** argv) {
    (void)argc;
    (void)argv;

    TestPositioner();
    TestScale();
    TestFrameSampler();
    TestTitleBar();
    TestTextBoundary();
    TestTextView();
    TestSyntax();
    TestSlider();
    TestPagination();
    TestNumberInput();
    TestOtpInput();
    TestSelect();
    TestDialog();
    TestSheet();
    TestMotion();
    TestScrollbar();
    TestThemeSettings();
    TestResizable();
    TestTree();
    TestCalendar();
    TestColorPicker();
    TestToast();
    TestVirtualList();
    TestDatePicker();
    TestPopup();
    TestTextSelection();
    TestRope();
    TestMaskPattern();
    TestUndoManager();
    TestInputState();
    TestList();
    TestPopupMenu();
    TestDataTable();
    TestDock();
    TestTab();
    TestSetting();
    TestSearchableList();
    TestSidebar();
    TestWindowBorder();
    TestAvatar();
    TestKbd();
    TestNativeMenu();
    TestTiles();
    TestRoot();
    TestSankey();
    TestJson();
    TestInspector();
    TestThemeColor();
    TestDockState();
    TestFocusTrap();
    TestKeymap();
    TestEventEmitter();
    TestListSettings();
    TestStateStyle();
    TestClick();

    if (gTestFailures == 0) {
        printf("ok: %d checks\n", gTestChecks);
        return 0;
    }
    printf("FAILED: %d of %d checks\n", gTestFailures, gTestChecks);
    return 1;
}
