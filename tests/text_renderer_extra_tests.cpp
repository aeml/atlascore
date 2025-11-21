/*
 * Copyright (C) 2025 aeml
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

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
