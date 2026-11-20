#pragma once

#include <cstdint>

namespace kernetile::input {

// ============================================================================
// USB HID Usage Tables (Usage Page 0x07: Keyboard / Keypad)
// 跨操作系统与硬件驱动的标准物理扫描码
// ============================================================================
enum class KeyCode : uint16_t {
    None          = 0x00,
    Key_A         = 0x04,
    Key_B         = 0x05,
    Key_C         = 0x06,
    Key_D         = 0x07,
    Key_E         = 0x08,
    Key_F         = 0x09,
    Key_G         = 0x0A,
    Key_H         = 0x0B,
    Key_I         = 0x0C,
    Key_J         = 0x0D,
    Key_K         = 0x0E,
    Key_L         = 0x0F,
    Key_M         = 0x10,
    Key_N         = 0x11,
    Key_O         = 0x12,
    Key_P         = 0x13,
    Key_Q         = 0x14,
    Key_R         = 0x15,
    Key_S         = 0x16,
    Key_T         = 0x17,
    Key_U         = 0x18,
    Key_V         = 0x19,
    Key_W         = 0x1A,
    Key_X         = 0x1B,
    Key_Y         = 0x1C,
    Key_Z         = 0x1D,

    Key_1         = 0x1E,
    Key_2         = 0x1F,
    Key_3         = 0x20,
    Key_4         = 0x21,
    Key_5         = 0x22,
    Key_6         = 0x23,
    Key_7         = 0x24,
    Key_8         = 0x25,
    Key_9         = 0x26,
    Key_0         = 0x27,

    Key_Enter     = 0x28,
    Key_Escape    = 0x29,
    Key_Backspace = 0x2A,
    Key_Tab       = 0x2B,
    Key_Space     = 0x2C,
    Key_Minus     = 0x2D,
    Key_Equal     = 0x2E,
    Key_LeftBracket  = 0x2F,
    Key_RightBracket = 0x30,
    Key_Backslash    = 0x31,
    Key_Semicolon    = 0x33,
    Key_Quote        = 0x34,
    Key_Grave        = 0x35,
    Key_Comma        = 0x36,
    Key_Period       = 0x37,
    Key_Slash        = 0x38,
    Key_CapsLock     = 0x39,

    Key_F1        = 0x3A,
    Key_F2        = 0x3B,
    Key_F3        = 0x3C,
    Key_F4        = 0x3D,
    Key_F5        = 0x3E,
    Key_F6        = 0x3F,
    Key_F7        = 0x40,
    Key_F8        = 0x41,
    Key_F9        = 0x42,
    Key_F10       = 0x43,
    Key_F11       = 0x44,
    Key_F12       = 0x45,

    Key_PrintScreen = 0x46,
    Key_ScrollLock  = 0x47,
    Key_Pause       = 0x48,
    Key_Insert      = 0x49,
    Key_Home        = 0x4A,
    Key_PageUp      = 0x4B,
    Key_Delete      = 0x4C,
    Key_End         = 0x4D,
    Key_PageDown    = 0x4E,
    Key_Right       = 0x4F,
    Key_Left        = 0x50,
    Key_Down        = 0x51,
    Key_Up          = 0x52,

    Key_LeftControl  = 0xE0,
    Key_LeftShift    = 0xE1,
    Key_LeftAlt      = 0xE2,
    Key_LeftMeta     = 0xE3,
    Key_RightControl = 0xE4,
    Key_RightShift   = 0xE5,
    Key_RightAlt     = 0xE6,
    Key_RightMeta    = 0xE7
};

// 平台原生按键码转换
inline KeyCode from_win32_vk(uint32_t vk) {
    if (vk >= 'A' && vk <= 'Z') {
        return static_cast<KeyCode>(static_cast<uint16_t>(KeyCode::Key_A) + (vk - 'A'));
    }
    if (vk >= '0' && vk <= '9') {
        if (vk == '0') return KeyCode::Key_0;
        return static_cast<KeyCode>(static_cast<uint16_t>(KeyCode::Key_1) + (vk - '1'));
    }
    if (vk >= 0x70 && vk <= 0x7B) { // VK_F1 ~ VK_F12
        return static_cast<KeyCode>(static_cast<uint16_t>(KeyCode::Key_F1) + (vk - 0x70));
    }
    switch (vk) {
        case 0x08: return KeyCode::Key_Backspace;
        case 0x09: return KeyCode::Key_Tab;
        case 0x0D: return KeyCode::Key_Enter;
        case 0x1B: return KeyCode::Key_Escape;
        case 0x20: return KeyCode::Key_Space;
        case 0x25: return KeyCode::Key_Left;
        case 0x26: return KeyCode::Key_Up;
        case 0x27: return KeyCode::Key_Right;
        case 0x28: return KeyCode::Key_Down;
        case 0x2D: return KeyCode::Key_Insert;
        case 0x2E: return KeyCode::Key_Delete;
        case 0x24: return KeyCode::Key_Home;
        case 0x23: return KeyCode::Key_End;
        case 0x21: return KeyCode::Key_PageUp;
        case 0x22: return KeyCode::Key_PageDown;
        case 0xA0: return KeyCode::Key_LeftShift;
        case 0xA1: return KeyCode::Key_RightShift;
        case 0xA2: return KeyCode::Key_LeftControl;
        case 0xA3: return KeyCode::Key_RightControl;
        case 0xA4: return KeyCode::Key_LeftAlt;
        case 0xA5: return KeyCode::Key_RightAlt;
        case 0x5B: return KeyCode::Key_LeftMeta;
        case 0x5C: return KeyCode::Key_RightMeta;
        default:   return KeyCode::None;
    }
}

} // namespace kernetile::input
