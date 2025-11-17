#include "ascii/TextRenderer.hpp"
#include <iostream>

namespace ascii
{
    TextSurface::TextSurface(int width, int height)
        : m_width(width), m_height(height), m_cells(static_cast<std::size_t>(width * height))
    {
        Clear();
    }

    void TextSurface::Clear(char fill)
    {
        for (auto &cell : m_cells)
        {
            cell.ch = fill;
        }
    }

    void TextSurface::Put(int x, int y, char ch)
    {
        if (x < 0 || y < 0 || x >= m_width || y >= m_height) return;
        m_cells[static_cast<std::size_t>(y * m_width + x)].ch = ch;
    }

    TextRenderer::TextRenderer(int width, int height)
        : m_current(width, height), m_previous(width, height)
    {
        m_previous.Clear('\0'); // ensure first diff treats all cells as changed
    }

    void TextRenderer::Clear(char fill)
    {
        m_current.Clear(fill);
    }

    void TextRenderer::Put(int x, int y, char ch)
    {
        m_current.Put(x, y, ch);
    }

    std::size_t TextRenderer::ComputeDiff() const
    {
        const Cell* cur = m_current.Data();
        const Cell* prev = m_previous.Data();
        std::size_t changed = 0;
        const std::size_t total = static_cast<std::size_t>(m_current.Width() * m_current.Height());
        for (std::size_t i = 0; i < total; ++i)
        {
            if (cur[i].ch != prev[i].ch) ++changed;
        }
        return changed;
    }

    std::size_t TextRenderer::PresentDiff(std::ostream& out)
    {
        const int w = m_current.Width();
        const int h = m_current.Height();
        const Cell* cur = m_current.Data();
        const Cell* prev = m_previous.Data();
        std::size_t changed = 0;

        // Save cursor position so we can render into our viewport and then
        // restore the terminal state for the caller (e.g., prompts/logs).
        out << "\x1b[s"; // save cursor

        constexpr int originRow = 1;
        constexpr int originCol = 1;
        for (int y = 0; y < h; ++y)
        {
            bool lineChanged = false;
            for (int x = 0; x < w; ++x)
            {
                const std::size_t idx = static_cast<std::size_t>(y * w + x);
                if (cur[idx].ch != prev[idx].ch)
                {
                    ++changed;
                    lineChanged = true;
                }
            }
            if (lineChanged)
            {
                // Re-render entire changed line for simplicity into our viewport
                const int row = originRow + y;
                out << "\x1b[" << row << ";" << originCol << "H"; // move cursor
                for (int x = 0; x < w; ++x)
                {
                    out << cur[static_cast<std::size_t>(y * w + x)].ch;
                }
            }
        }
        // Copy current to previous for next frame
        Cell* prevMut = m_previous.Data();
        const std::size_t total = static_cast<std::size_t>(w * h);
        for (std::size_t i = 0; i < total; ++i)
        {
            prevMut[i] = cur[i];
        }

        // Restore cursor to where it was before rendering.
        out << "\x1b[u";
        return changed;
    }
}
