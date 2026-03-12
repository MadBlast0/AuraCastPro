#pragma once
// =============================================================================
// KeyboardToInput.h — PC keyboard → device key event translation
// =============================================================================
#include <functional>
#include <cstdint>
#include <atomic>

namespace aura {

enum class DeviceOS { iOS, Android };

struct KeyEvent {
    uint32_t  deviceKeyCode; // Android KeyEvent keyCode or iOS HID usage
    uint32_t  keycode{0};    // Alias used by AndroidControlBridge
    int       action{0};     // 0=ACTION_DOWN, 1=ACTION_UP (Android)
    bool      isDown;        // true = key pressed, false = key released
    uint32_t  modifiers;     // Shift=1, Ctrl=2, Alt=4
    char      character{0};  // printable character (if any)
};

class KeyboardToInput {
public:
    using KeyCallback = std::function<void(const KeyEvent&)>;

    KeyboardToInput();
    void init();
    void start();
    void stop();
    void shutdown();

    // Called from WndProc WM_KEYDOWN / WM_KEYUP
    void onWin32Key(uint32_t vkCode, bool isDown, uint32_t modifiers);

    void setCallback(KeyCallback cb) { m_callback = std::move(cb); }
    void setTargetOS(DeviceOS os)    { m_targetOS = os; }
    void setEnabled(bool v)          { m_enabled.store(v); }

private:
    KeyCallback        m_callback;
    std::atomic<bool>  m_enabled{true};
    DeviceOS           m_targetOS{DeviceOS::Android};

    uint32_t win32ToAndroid(uint32_t vkCode) const;
    uint32_t win32ToiOS_HID(uint32_t vkCode) const;
};

} // namespace aura
