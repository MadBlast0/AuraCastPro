// =============================================================================
// MouseToTouch.cpp — Mouse → touch event translation
// =============================================================================
#include "../pch.h"  // PCH
#include "MouseToTouch.h"
#include "../utils/Logger.h"
#include <chrono>

namespace aura {

MouseToTouch::MouseToTouch() {}
MouseToTouch::~MouseToTouch() {}

void MouseToTouch::init() {
    AURA_LOG_INFO("MouseToTouch",
        "Initialised. Mapping: LClick→Tap, LDrag→Swipe, "
        "Scroll→2-finger scroll, RClick→LongPress.");
}

void MouseToTouch::start() { m_enabled.store(true); }
void MouseToTouch::stop()  { m_enabled.store(false); }
void MouseToTouch::shutdown() { stop(); }

TouchEvent MouseToTouch::makeEvent(TouchEventType type, int x, int y,
                                    int windowW, int windowH) const {
    TouchEvent e;
    e.type = type;
    e.x = windowW  > 0 ? static_cast<float>(x) / windowW  : 0.f;
    e.y = windowH  > 0 ? static_cast<float>(y) / windowH  : 0.f;
    // Clamp to [0,1]
    e.x = std::clamp(e.x, 0.f, 1.f);
    e.y = std::clamp(e.y, 0.f, 1.f);
    e.fingerId = 0;
    return e;
}

void MouseToTouch::onMouseDown(int x, int y, int w, int h) {
    if (!m_enabled.load() || !m_callback) return;
    m_dragging = true;
    m_lastX = x; m_lastY = y;
    m_callback(makeEvent(TouchEventType::Down, x, y, w, h));
}

void MouseToTouch::onMouseMove(int x, int y, int w, int h) {
    if (!m_enabled.load() || !m_callback || !m_dragging) return;
    auto e = makeEvent(TouchEventType::Move, x, y, w, h);
    // Compute delta in normalised space
    e.dx = w > 0 ? static_cast<float>(x - m_lastX) / w : 0.f;
    e.dy = h > 0 ? static_cast<float>(y - m_lastY) / h : 0.f;
    m_lastX = x; m_lastY = y;
    m_callback(e);
}

void MouseToTouch::onMouseUp(int x, int y, int w, int h) {
    if (!m_enabled.load() || !m_callback) return;
    m_dragging = false;
    m_callback(makeEvent(TouchEventType::Up, x, y, w, h));
}

void MouseToTouch::onMouseScroll(int x, int y, int delta, int w, int h) {
    if (!m_enabled.load() || !m_callback) return;
    auto e  = makeEvent(TouchEventType::Scroll, x, y, w, h);
    e.dy    = static_cast<float>(delta) / 120.f; // normalize wheel clicks
    m_callback(e);
}

void MouseToTouch::onRightClick(int x, int y, int w, int h) {
    if (!m_enabled.load() || !m_callback) return;
    m_callback(makeEvent(TouchEventType::LongPress, x, y, w, h));
}

} // namespace aura
