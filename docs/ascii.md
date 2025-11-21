# ASCII Renderer Module

The `ascii` module provides lightweight text-based visualization for the simulation. It is designed to render the simulation state to the terminal using ANSI escape codes, with optimizations for performance and minimal flicker.

## Components

### `TextRenderer`

The `TextRenderer` class is the core of the visualization system. It manages a double-buffered grid of characters (`TextSurface`) to efficiently update the terminal.

-   **Double Buffering**: Maintains a `current` and `previous` frame buffer.
-   **Diff-Based Rendering**: Only outputs characters that have changed since the last frame to minimize I/O and flicker.
-   **Primitives**: Supports drawing lines, rectangles, circles, and ellipses.
-   **Colors**: Supports basic ANSI colors via the `Color` enum.
-   **Headless Mode**: Can be toggled to suppress output while still tracking state (useful for testing).

### `Renderer`

The `Renderer` class provides a higher-level interface for rendering simulation entities.

-   **`RenderBodies`**: Takes a list of `TransformComponent` and `RigidBodyComponent` and renders them to the `TextRenderer` (or output stream).
-   **ECS Integration**: Designed to work with the ECS `World` to query and render entities.

## Usage

The `TextRenderer` is typically used within a `Scenario`'s `Render` method.

```cpp
// In a Scenario::Render method
void MyScenario::Render(ecs::World& world) {
    m_renderer.Clear();
    
    // Draw some text
    m_renderer.Put(10, 10, 'H', ascii::Color::Red);
    
    // Draw a shape
    m_renderer.DrawRect(5, 5, 20, 10, '#', ascii::Color::Blue);
    
    // Present the frame (calculates diff and outputs to stream)
    m_renderer.PresentDiff(std::cout);
}
```

## Future Improvements

-   **Sprite Support**: Rendering small ASCII sprites.
-   **Z-Ordering**: Basic depth handling for overlapping shapes.
-   **UI Elements**: Widgets for displaying stats and controls.
