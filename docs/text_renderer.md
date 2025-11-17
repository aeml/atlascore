# Text Renderer (Planned)

Goal: High-performance terminal-based renderer for AtlasCore scenarios.

## Concept

Maintain a 2D back buffer of characters + color attributes. Each frame:
1. Clear or apply decay/fade.
2. Draw primitives (glyphs, lines, boxes) via lightweight APIs.
3. Optional depth + shading: map Z/depth or light intensity to ASCII ramps (e.g., ` .:-=+*#%@`).
4. Flush diff vs previous frame to minimize terminal writes.

## Core Ideas

- Double buffering: `current` / `previous` char grids.
- Dirty region tracking: only emit changed cells.
- ANSI escape codes for color (foreground/background) and cursor positioning.
- Coordinate system: origin at top-left (y down) for simplicity; provide abstraction if needed.
- Batch API: accumulate draw calls (sprites composed of characters) before final commit.

## Minimal API Sketch

```cpp
class TextSurface {
public:
    TextSurface(int width, int height);
    void Clear(char fill=' ');
    void Put(int x, int y, char ch, Color fg=Color::White, Color bg=Color::Black);
    void Blit(int x, int y, const GlyphGrid& grid);
    void Shade(int x, int y, float depth, const ShadeRamp& ramp);
    void PresentDiff(std::ostream& out);
};
```

## Faux 3D / Shading

- Project simple 3D coords to 2D (orthographic or basic perspective).
- Depth -> ramp index; brightness or normal dot(light) -> color choice.
- Potential small lighting model: single directional light + ambient.

## Performance Considerations

- Avoid per-cell std::string allocations; store raw POD structs.
- Precompute ANSI sequences for common colors.
- Use contiguous memory for buffers (vector<char>, vector<ColorCode>). Parallelize shading if needed via Jobs.

## Future Extensions

- Sprite animation (time-based frame selection).
- Particle system (ASCII sparks with fade).
- Offscreen composition layers (UI overlay vs world).
- Optional write throttling to maintain deterministic timing.

This document guides future implementation; not yet active in codebase.
