#include "ascii/TextRenderer.hpp"
#include <iostream>
#include <cmath>

namespace ascii
{
    namespace
    {
        const char* GetColorCode(Color c)
        {
            switch (c)
            {
            case Color::Red:     return "\x1b[31m";
            case Color::Green:   return "\x1b[32m";
            case Color::Yellow:  return "\x1b[33m";
            case Color::Blue:    return "\x1b[34m";
            case Color::Magenta: return "\x1b[35m";
            case Color::Cyan:    return "\x1b[36m";
            case Color::White:   return "\x1b[37m";
            case Color::Default: 
            default:             return "\x1b[39m";
            }
        }
    }

    TextSurface::TextSurface(int width, int height)
        : m_width(width), m_height(height), m_cells(static_cast<std::size_t>(width * height))
    {
        Clear();
    }

    void TextSurface::Clear(char fill, Color color)
    {
        for (auto &cell : m_cells)
        {
            cell.ch = fill;
            cell.color = color;
        }
    }

    void TextSurface::Put(int x, int y, char ch, Color color)
    {
        if (x < 0 || y < 0 || x >= m_width || y >= m_height) return;
        auto& cell = m_cells[static_cast<std::size_t>(y * m_width + x)];
        cell.ch = ch;
        cell.color = color;
    }

    TextRenderer::TextRenderer(int width, int height)
        : m_current(width, height), m_previous(width, height)
    {
        // ensure first diff treats all cells as changed
        m_previous.Clear('\0', Color::Default); 
    }

    void TextRenderer::Clear(char fill, Color color)
    {
        m_current.Clear(fill, color);
    }

    void TextRenderer::Put(int x, int y, char ch, Color color)
    {
        m_current.Put(x, y, ch, color);
    }

    void TextRenderer::DrawLine(int x0, int y0, int x1, int y1, char ch, Color color)
    {
        int dx = std::abs(x1 - x0);
        int dy = std::abs(y1 - y0);
        int sx = (x0 < x1) ? 1 : -1;
        int sy = (y0 < y1) ? 1 : -1;
        int err = dx - dy;

        while (true)
        {
            Put(x0, y0, ch, color);
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 > -dy)
            {
                err -= dy;
                x0 += sx;
            }
            if (e2 < dx)
            {
                err += dx;
                y0 += sy;
            }
        }
    }

    void TextRenderer::DrawRect(int x, int y, int w, int h, char ch, Color color)
    {
        if (w <= 0 || h <= 0) return;
        DrawLine(x, y, x + w - 1, y, ch, color);
        DrawLine(x, y + h - 1, x + w - 1, y + h - 1, ch, color);
        DrawLine(x, y, x, y + h - 1, ch, color);
        DrawLine(x + w - 1, y, x + w - 1, y + h - 1, ch, color);
    }

    void TextRenderer::DrawCircle(int xc, int yc, int r, char ch, Color color)
    {
        int x = 0, y = r;
        int d = 3 - 2 * r;
        auto draw8 = [&](int xc, int yc, int x, int y) {
            Put(xc+x, yc+y, ch, color);
            Put(xc-x, yc+y, ch, color);
            Put(xc+x, yc-y, ch, color);
            Put(xc-x, yc-y, ch, color);
            Put(xc+y, yc+x, ch, color);
            Put(xc-y, yc+x, ch, color);
            Put(xc+y, yc-x, ch, color);
            Put(xc-y, yc-x, ch, color);
        };
        draw8(xc, yc, x, y);
        while (y >= x)
        {
            x++;
            if (d > 0)
            {
                y--;
                d = d + 4 * (x - y) + 10;
            }
            else
                d = d + 4 * x + 6;
            draw8(xc, yc, x, y);
        }
    }

    void TextRenderer::DrawEllipse(int xc, int yc, int rx, int ry, char ch, Color color)
    {
        long rx2 = (long)rx * rx;
        long ry2 = (long)ry * ry;
        long twoRx2 = 2 * rx2;
        long twoRy2 = 2 * ry2;
        long p;
        long x = 0;
        long y = ry;
        long px = 0;
        long py = twoRx2 * y;

        auto draw4 = [&](int xc, int yc, int x, int y) {
            Put(xc + x, yc + y, ch, color);
            Put(xc - x, yc + y, ch, color);
            Put(xc + x, yc - y, ch, color);
            Put(xc - x, yc - y, ch, color);
        };

        // Region 1
        draw4(xc, yc, x, y);
        p = (long)(ry2 - (rx2 * ry) + (0.25 * rx2));
        while (px < py) {
            x++;
            px += twoRy2;
            if (p < 0) {
                p += ry2 + px;
            } else {
                y--;
                py -= twoRx2;
                p += ry2 + px - py;
            }
            draw4(xc, yc, x, y);
        }

        // Region 2
        p = (long)(ry2 * (x + 0.5) * (x + 0.5) + rx2 * (y - 1) * (y - 1) - rx2 * ry2);
        while (y > 0) {
            y--;
            py -= twoRx2;
            if (p > 0) {
                p += rx2 - py;
            } else {
                x++;
                px += twoRy2;
                p += rx2 - py + px;
            }
            draw4(xc, yc, x, y);
        }
    }

    void TextRenderer::FillEllipse(int xc, int yc, int rx, int ry, char ch, Color color)
    {
        long rx2 = (long)rx * rx;
        long ry2 = (long)ry * ry;
        long twoRx2 = 2 * rx2;
        long twoRy2 = 2 * ry2;
        long p;
        long x = 0;
        long y = ry;
        long px = 0;
        long py = twoRx2 * y;

        auto drawLine = [&](int xc, int yc, int x, int y) {
            DrawLine(xc - x, yc + y, xc + x, yc + y, ch, color);
            DrawLine(xc - x, yc - y, xc + x, yc - y, ch, color);
        };

        // Region 1
        drawLine(xc, yc, x, y);
        p = (long)(ry2 - (rx2 * ry) + (0.25 * rx2));
        while (px < py) {
            x++;
            px += twoRy2;
            if (p < 0) {
                p += ry2 + px;
            } else {
                y--;
                py -= twoRx2;
                p += ry2 + px - py;
            }
            drawLine(xc, yc, x, y);
        }

        // Region 2
        p = (long)(ry2 * (x + 0.5) * (x + 0.5) + rx2 * (y - 1) * (y - 1) - rx2 * ry2);
        while (y > 0) {
            y--;
            py -= twoRx2;
            if (p > 0) {
                p += rx2 - py;
            } else {
                x++;
                px += twoRy2;
                p += rx2 - py + px;
            }
            drawLine(xc, yc, x, y);
        }
    }

    std::size_t TextRenderer::ComputeDiff() const
    {
        const Cell* cur = m_current.Data();
        const Cell* prev = m_previous.Data();
        std::size_t changed = 0;
        const std::size_t total = static_cast<std::size_t>(m_current.Width() * m_current.Height());
        for (std::size_t i = 0; i < total; ++i)
        {
            if (cur[i] != prev[i]) ++changed;
        }
        return changed;
    }

    std::size_t TextRenderer::PresentDiff(std::ostream& out)
    {
        if (m_headless)
        {
            const Cell* cur = m_current.Data();
            Cell* prev = m_previous.Data();
            const std::size_t total = static_cast<std::size_t>(m_current.Width() * m_current.Height());
            std::size_t changed = 0;
            for (std::size_t i = 0; i < total; ++i)
            {
                if (cur[i] != prev[i])
                {
                    ++changed;
                    prev[i] = cur[i];
                }
            }
            return changed;
        }

        const int w = m_current.Width();
        const int h = m_current.Height();
        const Cell* cur = m_current.Data();
        Cell* prev = m_previous.Data();
        std::size_t changed = 0;

        // Save cursor position
        out << "\x1b[s"; 
        // Hide cursor
        out << "\x1b[?25l";

        Color lastColor = Color::Default;
        // Ensure we start with default color
        out << GetColorCode(lastColor);

        int cursorX = -1;
        int cursorY = -1;

        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                const std::size_t idx = static_cast<std::size_t>(y * w + x);
                if (cur[idx] != prev[idx])
                {
                    ++changed;
                    
                    // Move cursor if not already at the right spot
                    if (cursorX != x || cursorY != y)
                    {
                        // ANSI uses 1-based coordinates
                        out << "\x1b[" << (y + 1) << ";" << (x + 1) << "H";
                    }

                    // Change color if needed
                    if (cur[idx].color != lastColor)
                    {
                        out << GetColorCode(cur[idx].color);
                        lastColor = cur[idx].color;
                    }

                    out << cur[idx].ch;
                    
                    // Update cursor position (it advanced by 1)
                    cursorX = x + 1;
                    cursorY = y;

                    // Update previous buffer
                    prev[idx] = cur[idx];
                }
            }
        }

        // Reset color
        out << "\x1b[0m";
        // Restore cursor
        out << "\x1b[u";
        // Show cursor
        out << "\x1b[?25h";
        
        out.flush(); // Ensure output is sent
        return changed;
    }
}
