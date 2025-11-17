#include "ascii/TextRenderer.hpp"
#include <cassert>
#include <sstream>
#include <iostream>

int main() {
    ascii::TextRenderer r(4,2);
    // Initial diff all cells.
    assert(r.ComputeDiff() == 8);
    std::stringstream s1; r.PresentDiff(s1);
    // Out-of-bounds writes ignored.
    r.Put(-1,0,'X'); r.Put(10,10,'Y');
    assert(r.ComputeDiff() == 0);
    // In-bounds change.
    r.Put(0,0,'A');
    assert(r.ComputeDiff() == 1);
    std::stringstream s2; r.PresentDiff(s2);
    // Clear should change all cells again (except they become space).
    r.Clear(' ');
    // After Clear, A reset to space -> one cell altered.
    assert(r.ComputeDiff() == 1);
    std::stringstream s3; r.PresentDiff(s3);
    std::cout << "Text renderer extra tests passed.\n";
    return 0;
}
