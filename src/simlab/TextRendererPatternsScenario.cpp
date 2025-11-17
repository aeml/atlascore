#include "simlab/Scenario.hpp"
#include "ascii/TextRenderer.hpp"
#include "core/Logger.hpp"

#include <cmath>
#include <iostream>
#include <vector>

namespace simlab {
namespace {

class TextRendererPatternsScenario final : public IScenario {
public:
    void Setup(ecs::World& /*world*/) override {
        m_logger.Info("[simlab] Setup TextRenderer patterns scenario");
        // Initialize a text surface
        m_width = 80;
        m_height = 24;
        m_renderer = std::make_unique<ascii::TextRenderer>(m_width, m_height);
        m_time = 0.0f;
    }

    void Step(ecs::World& /*world*/, float dt) override {
        m_time += dt;
        drawPatterns(m_time);
        m_renderer->PresentDiff(std::cout);
    }

private:
    void clear(char ch = ' ') { m_renderer->Clear(ch); }
    void put(int x, int y, char ch) { m_renderer->Put(x, y, ch); }

    void drawPatterns(float t) {
        clear(' ');
        // Sine wave across the screen
        for (int x = 0; x < m_width; ++x) {
            float fx = static_cast<float>(x);
            float y = (std::sin((fx * 0.15f) + t * 2.0f) * 8.0f) + (m_height / 2.0f);
            put(x, static_cast<int>(y), '*');
        }

        // Bouncing dots
        for (int i = 0; i < 5; ++i) {
            float phase = t * (1.0f + 0.2f * i);
            int x = static_cast<int>((std::sin(phase + i) * 0.5f + 0.5f) * (m_width - 1));
            int y = static_cast<int>((std::cos(phase * 1.3f + i) * 0.5f + 0.5f) * (m_height - 1));
            put(x, y, 'o');
        }

        // Lissajous curve
        for (int i = 0; i < 200; ++i) {
            float a = 3.0f;
            float b = 2.0f;
            float u = (i / 200.0f) * 6.28318f; // 2*pi
            int x = static_cast<int>(((std::sin(a * u + t) * 0.5f) + 0.5f) * (m_width - 1));
            int y = static_cast<int>(((std::sin(b * u + t * 0.7f) * 0.5f) + 0.5f) * (m_height - 1));
            put(x, y, '+');
        }

        // Border
        for (int x = 0; x < m_width; ++x) { put(x, 0, '-'); put(x, m_height-1, '-'); }
        for (int y = 0; y < m_height; ++y) { put(0, y, '|'); put(m_width-1, y, '|'); }
        put(0,0,'+'); put(m_width-1,0,'+'); put(0,m_height-1,'+'); put(m_width-1,m_height-1,'+');
    }

private:
    core::Logger m_logger;
    std::unique_ptr<ascii::TextRenderer> m_renderer;
    int m_width{0};
    int m_height{0};
    float m_time{0.0f};
};

} // namespace

std::unique_ptr<IScenario> CreateTextRendererPatternsScenario() {
    return std::make_unique<TextRendererPatternsScenario>();
}

// Auto-register under categories requested
static ScenarioAutoRegister g_reg_text_patterns{
    "text_patterns",
    "Text renderer patterns",
    &CreateTextRendererPatternsScenario,
    "Text Tests",
    "Text Renderer Tests"
};

} // namespace simlab
