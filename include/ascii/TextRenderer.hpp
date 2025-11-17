#pragma once

#include <vector>
#include <ostream>
#include <cstddef>
#include <algorithm>

namespace ascii
{
    struct Cell
    {
        char ch;
    };

    class TextSurface
    {
    public:
        TextSurface(int width, int height);
        void Clear(char fill = ' ');
        void Put(int x, int y, char ch);
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
        void Clear(char fill = ' ');
        void Put(int x, int y, char ch);
        // Compute number of changed cells vs previous frame (no output side effects).
        std::size_t ComputeDiff() const;
        // Present diff to stream and update previous buffer; returns changed cell count.
        std::size_t PresentDiff(std::ostream& out);
    private:
        TextSurface m_current;
        TextSurface m_previous;
    };
}
