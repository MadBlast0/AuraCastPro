// =============================================================================
// KeyboardToInput.cpp — Win32 VK codes → Android/iOS key events
// =============================================================================
#include "../pch.h"  // PCH
#include "KeyboardToInput.h"
#include "../utils/Logger.h"

// Android KeyEvent keycodes (subset)
namespace AKEYCODE {
    constexpr uint32_t BACK        = 4;
    constexpr uint32_t HOME        = 3;
    constexpr uint32_t VOLUME_UP   = 24;
    constexpr uint32_t VOLUME_DOWN = 25;
    constexpr uint32_t ENTER       = 66;
    constexpr uint32_t DEL         = 67;  // Backspace
    constexpr uint32_t SPACE       = 62;
    constexpr uint32_t TAB         = 61;
    constexpr uint32_t ESCAPE      = 111;
    constexpr uint32_t DPAD_UP     = 19;
    constexpr uint32_t DPAD_DOWN   = 20;
    constexpr uint32_t DPAD_LEFT   = 21;
    constexpr uint32_t DPAD_RIGHT  = 22;
}

namespace aura {

KeyboardToInput::KeyboardToInput() {}

void KeyboardToInput::init() {
    AURA_LOG_INFO("KeyboardToInput",
        "Initialised. VK code mapping: Win32 → Android KeyEvent / iOS HID. "
        "Special keys: Backspace→DEL, Enter→ENTER, Escape→BACK.");
}
void KeyboardToInput::start()    { m_enabled.store(true); }
void KeyboardToInput::stop()     { m_enabled.store(false); }
void KeyboardToInput::shutdown() { stop(); }

void KeyboardToInput::onWin32Key(uint32_t vkCode, bool isDown, uint32_t modifiers) {
    if (!m_enabled.load() || !m_callback) return;

    KeyEvent e;
    e.isDown    = isDown;
    e.modifiers = modifiers;

    if (m_targetOS == DeviceOS::Android) {
        e.deviceKeyCode = win32ToAndroid(vkCode);
    } else {
        e.deviceKeyCode = win32ToiOS_HID(vkCode);
    }

    if (e.deviceKeyCode != 0) m_callback(e);
}

uint32_t KeyboardToInput::win32ToAndroid(uint32_t vk) const {
    // VK_A..VK_Z (0x41..0x5A) → AKEYCODE_A..Z (29..54)
    if (vk >= 0x41 && vk <= 0x5A) return vk - 0x41 + 29;
    // VK_0..VK_9 (0x30..0x39) → AKEYCODE_0..9 (7..16)
    if (vk >= 0x30 && vk <= 0x39) return vk - 0x30 + 7;

    switch (vk) {
        case 0x08: return AKEYCODE::DEL;
        case 0x09: return AKEYCODE::TAB;
        case 0x0D: return AKEYCODE::ENTER;
        case 0x1B: return AKEYCODE::BACK;
        case 0x20: return AKEYCODE::SPACE;
        case 0x25: return AKEYCODE::DPAD_LEFT;
        case 0x26: return AKEYCODE::DPAD_UP;
        case 0x27: return AKEYCODE::DPAD_RIGHT;
        case 0x28: return AKEYCODE::DPAD_DOWN;
        // Extended navigation keys
        case 0x2E: return AKEYCODE::FORWARD_DEL;   // VK_DELETE
        case 0x24: return AKEYCODE::MOVE_HOME;      // VK_HOME
        case 0x23: return AKEYCODE::MOVE_END;       // VK_END
        case 0x21: return AKEYCODE::PAGE_UP;        // VK_PRIOR
        case 0x22: return AKEYCODE::PAGE_DOWN;      // VK_NEXT
        // Function keys (F1–F12)
        case 0x70: return AKEYCODE::F1;   case 0x71: return AKEYCODE::F2;
        case 0x72: return AKEYCODE::F3;   case 0x73: return AKEYCODE::F4;
        case 0x74: return AKEYCODE::F5;   case 0x75: return AKEYCODE::F6;
        case 0x76: return AKEYCODE::F7;   case 0x77: return AKEYCODE::F8;
        case 0x78: return AKEYCODE::F9;   case 0x79: return AKEYCODE::F10;
        case 0x7A: return AKEYCODE::F11;  case 0x7B: return AKEYCODE::F12;
        // Volume keys (physical keyboard media keys)
        case 0xAE: return AKEYCODE::VOLUME_DOWN;    // VK_VOLUME_DOWN
        case 0xAF: return AKEYCODE::VOLUME_UP;      // VK_VOLUME_UP
        case 0xAD: return AKEYCODE::VOLUME_MUTE;    // VK_VOLUME_MUTE
        default:   return 0; // unmapped
    }
}

uint32_t KeyboardToInput::win32ToiOS_HID(uint32_t vk) const {
    // iOS uses USB HID Usage Table page 0x07 (Keyboard)
    if (vk >= 0x41 && vk <= 0x5A) return vk - 0x41 + 0x04; // HID a=0x04
    if (vk >= 0x30 && vk <= 0x39) return vk == 0x30 ? 0x27 : vk - 0x31 + 0x1E;
    switch (vk) {
        case 0x08: return 0x2A; // Backspace
        case 0x0D: return 0x28; // Enter
        case 0x1B: return 0x29; // Escape
        case 0x20: return 0x2C; // Space
        case 0x09: return 0x2B; // Tab
        // Extended navigation (HID page 0x07)
        case 0x2E: return 0x4C; // Delete (forward)  VK_DELETE
        case 0x24: return 0x4A; // Home              VK_HOME
        case 0x23: return 0x4D; // End               VK_END
        case 0x21: return 0x4B; // Page Up           VK_PRIOR
        case 0x22: return 0x4E; // Page Down         VK_NEXT
        case 0x25: return 0x50; // Left Arrow        VK_LEFT
        case 0x26: return 0x52; // Up Arrow          VK_UP
        case 0x27: return 0x4F; // Right Arrow       VK_RIGHT
        case 0x28: return 0x51; // Down Arrow        VK_DOWN
        // Function keys F1-F12 (HID 0x3A-0x45)
        case 0x70: return 0x3A; case 0x71: return 0x3B; // F1, F2
        case 0x72: return 0x3C; case 0x73: return 0x3D; // F3, F4
        case 0x74: return 0x3E; case 0x75: return 0x3F; // F5, F6
        case 0x76: return 0x40; case 0x77: return 0x41; // F7, F8
        case 0x78: return 0x42; case 0x79: return 0x43; // F9, F10
        case 0x7A: return 0x44; case 0x7B: return 0x45; // F11, F12
        default:   return 0;
    }
}

} // namespace aura
