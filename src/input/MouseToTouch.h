#pragma once
// =============================================================================
// MouseToTouch.h — Translates PC mouse/trackpad events into device touch events
// =============================================================================
#include <functional>
#include <cstdint>
#include <atomic>

namespace aura {

enum class TouchEventType { Down, Move, Up, Scroll, LongPress };

struct TouchEvent {
    TouchEventType type;
    float x{0};     // Normalized [0,1] — scaled to device resolution
    float y{0};
    float dx{0};    // Delta for scroll
    float dy{0};
    int   fingerId{0};
};

class MouseToTouch {
public:
    using TouchCallback = std::function<void(const TouchEvent&)>;

    MouseToTouch();
    ~MouseToTouch();

    void init();
    void start();
    void stop();
    void shutdown();

    // Called by the Win32 WndProc with WM_LBUTTONDOWN/MOVE/UP/MOUSEWHEEL
    void onMouseDown(int x, int y, int windowW, int windowH);
    void onMouseMove(int x, int y, int windowW, int windowH);
    void onMouseUp(int x, int y, int windowW, int windowH);
    void onMouseScroll(int x, int y, int delta, int windowW, int windowH);
    void onRightClick(int x, int y, int windowW, int windowH); // → long press

    void setCallback(TouchCallback cb) { m_callback = std::move(cb); }
    void setEnabled(bool v)            { m_enabled.store(v); }

private:
    TouchCallback      m_callback;
    std::atomic<bool>  m_enabled{true};
    bool               m_dragging{false};
    int                m_lastX{0}, m_lastY{0};

    TouchEvent makeEvent(TouchEventType type, int x, int y,
                         int windowW, int windowH) const;
};

} // namespace aura
