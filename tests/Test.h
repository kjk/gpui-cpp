/* The whole test framework: utassert(cond), a pass counter, and a failure
   report. Ported tests live next to it, one file per Rust module.

   A test is a plain function. It is not registered anywhere; TestsMain() in
   tests.cpp calls it. That is enough for a tree this size and keeps the
   harness to the few lines below. */

#include "gpui.h"

using namespace gpui;

// Counted by every utassert, whether it passed or not.
extern int gTestChecks;
extern int gTestFailures;
// The suite currently running, so a failure line says where it came from.
extern const char* gTestSuite;

void TestFailed(const char* cond, const char* file, int line);
void TestSuite(const char* name);

#define utassert(cond)                                 \
    do {                                               \
        gTestChecks++;                                 \
        if (!(cond)) {                                 \
            TestFailed(#cond, __FILE__, __LINE__);     \
        }                                              \
    } while (0)

// Floats come out of layout arithmetic, so most comparisons are approximate.
// The tolerance is a thousandth of a pixel: tight enough that a wrong formula
// fails, loose enough that the order of two additions does not.
bool TestNear(float a, float b);

#define utassertnear(a, b) utassert(TestNear((a), (b)))

void TestPositioner();
void TestScale();
void TestFrameSampler();
void TestTitleBar();
void TestTextBoundary();
void TestSlider();
void TestPagination();
void TestNumberInput();
void TestOtpInput();
void TestSelect();
void TestDialog();
void TestSheet();
void TestRope();
void TestMaskPattern();
void TestUndoManager();
void TestInputState();
