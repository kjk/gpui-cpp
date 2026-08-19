/* Ported from crates/base/src/scrollbar.rs.
 *
 * Rust's own cases there drive a window and a drag; the arithmetic underneath
 * is the thumb's length and the two ways a pointer sets the offset — a track
 * press, which centres the thumb where it landed, and a drag, which keeps the
 * grab point. Offsets here run positive-down, the way El::ScrollY takes them;
 * Rust's run negative because it offsets the content rather than the view. */

#include "Test.h"

static void TheThumbShrinksWithWhatIsVisible() {
    // Half the content visible: half the track.
    utassertnear(ScrollbarThumbSize(400, 200, 400), 200.f);
    utassertnear(ScrollbarThumbSize(400, 100, 400), 100.f);
    // A very long document stops at the floor rather than vanishing.
    utassertnear(ScrollbarThumbSize(400, 100, 100000), 48.f);
    // And never exceeds the track it runs in.
    utassertnear(ScrollbarThumbSize(30, 100, 200), 30.f);
}

static void TheThumbSitsWhereTheOffsetSaysAndStopsAtTheEnds() {
    // Track 400, thumb 100, so 300 of travel over 600 of scroll.
    utassertnear(ScrollbarThumbPos(400, 100, 0, 200, 800), 0.f);
    utassertnear(ScrollbarThumbPos(400, 100, 600, 200, 800), 300.f);
    utassertnear(ScrollbarThumbPos(400, 100, 300, 200, 800), 150.f);
    // Past the end it stays put rather than running off the track.
    utassertnear(ScrollbarThumbPos(400, 100, 5000, 200, 800), 300.f);
    // Nothing to scroll, nothing to move.
    utassertnear(ScrollbarThumbPos(400, 400, 0, 800, 800), 0.f);
}

static void ATrackPressCentresTheThumbOnIt() {
    // Press halfway down a 400 track whose origin is 0: the thumb's centre
    // goes there, so its top lands at 150 of the 300 available -> half.
    utassertnear(ScrollbarOffsetForTrackPress(200, 0, 400, 100, 200, 800),
                 300.f);
    // At the very top the clamp holds it at zero.
    utassertnear(ScrollbarOffsetForTrackPress(0, 0, 400, 100, 200, 800), 0.f);
    // At the bottom, the full distance.
    utassertnear(ScrollbarOffsetForTrackPress(400, 0, 400, 100, 200, 800),
                 600.f);
    // The track's origin is subtracted, so an inset bar answers the same.
    utassertnear(ScrollbarOffsetForTrackPress(250, 50, 400, 100, 200, 800),
                 300.f);
}

static void ADragKeepsTheGrabPoint() {
    // Grabbed 100 into a 100-tall thumb... which is its bottom edge: dragging
    // to 250 puts the thumb's top at 150, half of the travel.
    utassertnear(ScrollbarOffsetForDrag(250, 100, 0, 400, 100, 200, 800),
                 300.f);
    // Grabbed at the top edge instead, the same thumb position needs the
    // pointer 100 higher.
    utassertnear(ScrollbarOffsetForDrag(150, 0, 0, 400, 100, 200, 800), 300.f);
    // Dragging past either end clamps rather than overshooting.
    utassertnear(ScrollbarOffsetForDrag(-500, 0, 0, 400, 100, 200, 800), 0.f);
    utassertnear(ScrollbarOffsetForDrag(5000, 0, 0, 400, 100, 200, 800), 600.f);
}

static void NothingToScrollMeansNoOffset() {
    utassertnear(ScrollbarOffsetForDrag(200, 0, 0, 400, 400, 800, 800), 0.f);
    utassertnear(ScrollbarOffsetForTrackPress(200, 0, 400, 400, 800, 800), 0.f);
}

void TestScrollbar() {
    TestSuite("scrollbar");
    TheThumbShrinksWithWhatIsVisible();
    TheThumbSitsWhereTheOffsetSaysAndStopsAtTheEnds();
    ATrackPressCentresTheThumbOnIt();
    ADragKeepsTheGrabPoint();
    NothingToScrollMeansNoOffset();
}
