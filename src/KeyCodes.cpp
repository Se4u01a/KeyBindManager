#include "KeyCodes.h"

#include <array>

namespace
{
    constexpr std::uint32_t MENU_TOGGLE_KEY = 0x3D;  // F3

    constexpr auto kKnownKeys = std::to_array<KeyCodes::KeyDescriptor>({
        {"Escape", "Esc", 0x01},
        {"Digit1", "1", 0x02},
        {"Digit2", "2", 0x03},
        {"Digit3", "3", 0x04},
        {"Digit4", "4", 0x05},
        {"Digit5", "5", 0x06},
        {"Digit6", "6", 0x07},
        {"Digit7", "7", 0x08},
        {"Digit8", "8", 0x09},
        {"Digit9", "9", 0x0A},
        {"Digit0", "0", 0x0B},
        {"Minus", "-", 0x0C},
        {"Equal", "=", 0x0D},
        {"Backspace", "Backspace", 0x0E},
        {"Tab", "Tab", 0x0F},
        {"KeyQ", "Q", 0x10},
        {"KeyW", "W", 0x11},
        {"KeyE", "E", 0x12},
        {"KeyR", "R", 0x13},
        {"KeyT", "T", 0x14},
        {"KeyY", "Y", 0x15},
        {"KeyU", "U", 0x16},
        {"KeyI", "I", 0x17},
        {"KeyO", "O", 0x18},
        {"KeyP", "P", 0x19},
        {"BracketLeft", "[", 0x1A},
        {"BracketRight", "]", 0x1B},
        {"Enter", "Enter", 0x1C},
        {"ControlLeft", "Left Ctrl", 0x1D},
        {"ControlRight", "Right Ctrl", 0x9D},
        {"KeyA", "A", 0x1E},
        {"KeyS", "S", 0x1F},
        {"KeyD", "D", 0x20},
        {"KeyF", "F", 0x21},
        {"KeyG", "G", 0x22},
        {"KeyH", "H", 0x23},
        {"KeyJ", "J", 0x24},
        {"KeyK", "K", 0x25},
        {"KeyL", "L", 0x26},
        {"Semicolon", ";", 0x27},
        {"Quote", "'", 0x28},
        {"Backquote", "`", 0x29},
        {"ShiftLeft", "Left Shift", 0x2A},
        {"Backslash", "\\", 0x2B},
        {"KeyZ", "Z", 0x2C},
        {"KeyX", "X", 0x2D},
        {"KeyC", "C", 0x2E},
        {"KeyV", "V", 0x2F},
        {"KeyB", "B", 0x30},
        {"KeyN", "N", 0x31},
        {"KeyM", "M", 0x32},
        {"Comma", ",", 0x33},
        {"Period", ".", 0x34},
        {"Slash", "/", 0x35},
        {"ShiftRight", "Right Shift", 0x36},
        {"NumpadMultiply", "Num *", 0x37},
        {"AltLeft", "Left Alt", 0x38},
        {"AltRight", "Right Alt", 0xB8},
        {"Space", "Space", 0x39},
        {"CapsLock", "Caps Lock", 0x3A},
        {"F1", "F1", 0x3B},
        {"F2", "F2", 0x3C},
        {"F3", "F3", 0x3D},
        {"F4", "F4", 0x3E},
        {"F5", "F5", 0x3F},
        {"F6", "F6", 0x40},
        {"F7", "F7", 0x41},
        {"F8", "F8", 0x42},
        {"F9", "F9", 0x43},
        {"F10", "F10", 0x44},
        {"NumLock", "Num Lock", 0x45},
        {"ScrollLock", "Scroll Lock", 0x46},
        {"Numpad7", "Num 7", 0x47},
        {"Numpad8", "Num 8", 0x48},
        {"Numpad9", "Num 9", 0x49},
        {"NumpadSubtract", "Num -", 0x4A},
        {"Numpad4", "Num 4", 0x4B},
        {"Numpad5", "Num 5", 0x4C},
        {"Numpad6", "Num 6", 0x4D},
        {"NumpadAdd", "Num +", 0x4E},
        {"Numpad1", "Num 1", 0x4F},
        {"Numpad2", "Num 2", 0x50},
        {"Numpad3", "Num 3", 0x51},
        {"Numpad0", "Num 0", 0x52},
        {"NumpadDecimal", "Num Del", 0x53},
        {"NumpadEnter", "Num Enter", 0x9C},
        {"NumpadDivide", "Num /", 0xB5},
        {"Pause", "Pause", 0xC5},
        {"PrintScreen", "Print Screen", 0xB7},
        {"Home", "Home", 0xC7},
        {"ArrowUp", "Up Arrow", 0xC8},
        {"PageUp", "Page Up", 0xC9},
        {"ArrowLeft", "Left Arrow", 0xCB},
        {"ArrowRight", "Right Arrow", 0xCD},
        {"End", "End", 0xCF},
        {"ArrowDown", "Down Arrow", 0xD0},
        {"PageDown", "Page Down", 0xD1},
        {"Insert", "Insert", 0xD2},
        {"Delete", "Delete", 0xD3},
        {"Mouse3", "Mouse3", 0x100},
        {"Mouse4", "Mouse4", 0x101},
        {"Mouse5", "Mouse5", 0x102},
    });
}

std::optional<std::uint32_t> KeyCodes::JsCodeToDxScanCode(const std::string_view jsCode)
{
    for (const auto& key : kKnownKeys) {
        if (key.jsCode == jsCode) {
            return key.dxScanCode;
        }
    }

    return std::nullopt;
}

std::string KeyCodes::DxScanCodeToDisplayName(const std::uint32_t dxScanCode)
{
    for (const auto& key : kKnownKeys) {
        if (key.dxScanCode == dxScanCode) {
            return std::string(key.displayName);
        }
    }

    return "Unassigned";
}

std::string KeyCodes::DxScanCodeToJsCode(const std::uint32_t dxScanCode)
{
    for (const auto& key : kKnownKeys) {
        if (key.dxScanCode == dxScanCode) {
            return std::string(key.jsCode);
        }
    }

    return {};
}

std::string KeyCodes::NormalizeDisplayName(const std::string_view displayName)
{
    if (displayName.empty()) {
        return {};
    }

    std::string normalized(displayName);
    if (normalized == " ") {
        return "Space";
    }

    if (normalized == "Esc") {
        return "Esc";
    }

    if (normalized == "ArrowUp") {
        return "Up Arrow";
    }
    if (normalized == "ArrowDown") {
        return "Down Arrow";
    }
    if (normalized == "ArrowLeft") {
        return "Left Arrow";
    }
    if (normalized == "ArrowRight") {
        return "Right Arrow";
    }
    if (normalized == "Middle Mouse") {
        return "Mouse3";
    }
    if (normalized == "BrowserBack") {
        return "Mouse4";
    }
    if (normalized == "BrowserForward") {
        return "Mouse5";
    }
    if (normalized == "Mouse4" || normalized == "Back Mouse") {
        return "Mouse4";
    }
    if (normalized == "Mouse5" || normalized == "Forward Mouse") {
        return "Mouse5";
    }

    if (normalized.size() == 1) {
        normalized[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(normalized[0])));
    }

    return normalized;
}

bool KeyCodes::IsReservedHotkey(const std::uint32_t dxScanCode)
{
    return dxScanCode == MENU_TOGGLE_KEY;
}
