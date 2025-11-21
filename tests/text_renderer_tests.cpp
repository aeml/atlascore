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

int main()
{
    ascii::TextRenderer renderer(8, 4);

    // First diff: all cells should be considered changed (previous initialized to '\0').
    std::size_t initialChanged = renderer.ComputeDiff();
    assert(initialChanged == 8 * 4);
    (void)initialChanged;

    std::stringstream ss;
    std::size_t flushedInitial = renderer.PresentDiff(ss);
    assert(flushedInitial == 8 * 4);
    (void)flushedInitial;

    // Modify a couple of cells.
    renderer.Put(0, 0, 'A');
    renderer.Put(7, 3, 'Z');
    renderer.Put(3, 2, '#');

    // Expect exactly 3 changes.
    std::size_t diffCount = renderer.ComputeDiff();
    assert(diffCount == 3);
    (void)diffCount;

    std::stringstream ss2;
    std::size_t flushed = renderer.PresentDiff(ss2);
    assert(flushed == 3);
    (void)flushed;

    // Further modification: overwrite one cell and add a new one.
    renderer.Put(3, 2, '@'); // changed from '#'
    renderer.Put(4, 1, 'X');

    std::size_t diffCount2 = renderer.ComputeDiff();
    assert(diffCount2 == 2);
    (void)diffCount2;

    std::stringstream ss3;
    std::size_t flushed2 = renderer.PresentDiff(ss3);
    assert(flushed2 == 2);
    (void)flushed2;

    std::cout << "Text renderer tests passed.\n";
    return 0;
}
