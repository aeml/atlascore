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

#pragma once

#include <vector>
#include <ostream>
#include <cstddef>
#include <algorithm>

namespace ascii
{
    enum class Color { Default, White, Red, Green, Blue, Yellow, Cyan, Magenta };

    struct Cell
    {
        char ch;
        Color color{Color::Default};
        
        bool operator==(const Cell& other) const {
            return ch == other.ch && color == other.color;
        }
        bool operator!=(const Cell& other) const {
            return !(*this == other);
        }
    };

    class TextSurface
    {
    public:
        TextSurface(int width, int height);
        void Clear(char fill = ' ', Color color = Color::Default);
        void Put(int x, int y, char ch, Color color = Color::Default);
        int Width() const { return m_width; }
        int Height() const { return m_height; }
        const Cell* Data() const { return m_cells.data(); }
        Cell* Data() { return m_cells.data(); }
    private:
        int m_width;
        int m_height;
        std::vector<Cell> m_cells;
    };

    class TextRenderer
    {
    public:
        TextRenderer(int width, int height);
        void Clear(char fill = ' ', Color color = Color::Default);
        void Put(int x, int y, char ch, Color color = Color::Default);
        
        // Primitives
        void DrawLine(int x0, int y0, int x1, int y1, char ch, Color color = Color::Default);
        void DrawRect(int x, int y, int w, int h, char ch, Color color = Color::Default);
        void DrawCircle(int xc, int yc, int r, char ch, Color color = Color::Default);
        void DrawEllipse(int xc, int yc, int rx, int ry, char ch, Color color = Color::Default);
        void FillEllipse(int xc, int yc, int rx, int ry, char ch, Color color = Color::Default);

        // Compute number of changed cells vs previous frame (no output side effects).
        std::size_t ComputeDiff() const;
        // Present diff to stream and update previous buffer; returns changed cell count.
        std::size_t PresentDiff(std::ostream& out);

        // Present full frame regardless of diff state (does not alter headless flag logic).
        // Returns number of changed cells vs previous (same metric as ComputeDiff) and updates previous.
        std::size_t PresentFull(std::ostream& out);

        void SetHeadless(bool headless) { m_headless = headless; }
        bool IsHeadless() const noexcept { return m_headless; }

    private:
        TextSurface m_current;
        TextSurface m_previous;
        bool m_headless{false};
    };
}
