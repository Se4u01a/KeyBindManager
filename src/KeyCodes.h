#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace KeyCodes
{
    struct KeyDescriptor
    {
        std::string_view jsCode;
        std::string_view displayName;
        std::uint32_t dxScanCode;
    };

    [[nodiscard]] std::optional<std::uint32_t> JsCodeToDxScanCode(std::string_view jsCode);
    [[nodiscard]] std::string DxScanCodeToDisplayName(std::uint32_t dxScanCode);
    [[nodiscard]] std::string DxScanCodeToJsCode(std::uint32_t dxScanCode);
    [[nodiscard]] std::string NormalizeDisplayName(std::string_view displayName);
    [[nodiscard]] bool IsReservedHotkey(std::uint32_t dxScanCode);
}
