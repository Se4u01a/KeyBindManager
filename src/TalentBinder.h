#pragma once

#include "keyhandler/keyhandler.h"

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace RE
{
    class SpellItem;
    class TESShout;
}

class TalentBinder
{
public:
    struct TalentEntry
    {
        std::uint32_t formID = 0;
        std::string name;
        std::string categoryLabel;
        std::string typeLabel;
    };

    static TalentBinder* GetSingleton();

    void Initialize();
    void RefreshMenu();
    void HandleUIMessage(const std::string& message);
    [[nodiscard]] std::uint32_t GetMenuHotkey() const;
    [[nodiscard]] std::string GetMenuHotkeyLabel() const;
    [[nodiscard]] bool IsMenuHotkey(std::uint32_t dxScanCode) const;

private:
    enum class ActiveActionType : std::uint8_t
    {
        kNone,
        kShout
    };

    struct ActiveAction
    {
        std::uint32_t formID = 0;
        ActiveActionType type = ActiveActionType::kNone;
    };

    struct BindingInfo
    {
        std::uint32_t dxScanCode = 0;
        std::uint32_t modifierDxScanCode = 0;
        std::string displayLabel;
    };

    TalentBinder() = default;

    [[nodiscard]] std::filesystem::path GetConfigPath() const;
    [[nodiscard]] std::vector<TalentEntry> CollectTalents() const;
    [[nodiscard]] std::string BuildSyncPayload(const std::string& statusMessage = {}) const;
    [[nodiscard]] bool ActivateTalent(std::uint32_t formID) const;
    [[nodiscard]] bool TriggerShout(RE::TESShout* shout, std::uint32_t triggerDxScanCode) const;
    [[nodiscard]] RE::SpellItem* LookupTalent(std::uint32_t formID) const;

    void LoadBindings();
    void PersistBindings() const;
    void RebuildHotkeyRegistrations();
    void BeginBindingCapture(std::uint32_t formID);
    void BeginMenuKeyCapture();
    void ClearPendingCapture();
    [[nodiscard]] bool HandleCapturedInput(std::uint32_t dxScanCode, KeyEventType eventType);
    void SetBinding(std::uint32_t formID, std::uint32_t dxScanCode, std::uint32_t modifierDxScanCode, std::string displayLabel);
    void ClearBinding(std::uint32_t formID);
    void SetMenuHotkey(std::uint32_t dxScanCode);
    [[nodiscard]] bool IsForbiddenMenuHotkey(std::uint32_t dxScanCode) const;
    [[nodiscard]] bool HasMenuHotkeyConflictUnlocked(std::uint32_t dxScanCode) const;
    [[nodiscard]] bool HasMenuHotkeyConflict(std::uint32_t dxScanCode) const;
    [[nodiscard]] std::string GetMenuHotkeyConflictReason(std::uint32_t dxScanCode) const;
    void OnBoundHotkeyPressed(std::uint32_t dxScanCode);
    void OnBoundHotkeyReleased(std::uint32_t dxScanCode);
    void PushSync(const std::string& statusMessage = {}) const;

    static std::optional<std::uint32_t> ParseFormID(std::string_view text);
    static std::string FormatFormID(std::uint32_t formID);
    static std::string EscapeJson(std::string_view value);
    static std::uint64_t MakeBindingKey(std::uint32_t dxScanCode, std::uint32_t modifierDxScanCode);

    mutable std::mutex _mutex;
    bool _initialized = false;
    bool _pendingMenuKeyCapture = false;
    std::optional<std::uint32_t> _pendingTalentBinding;
    std::optional<std::uint32_t> _pendingModifierBinding;
    std::unordered_set<std::uint32_t> _heldModifierKeys;
    std::uint32_t _menuHotkey = 0x3D;
    std::unordered_map<std::uint32_t, BindingInfo> _bindingsByTalent;
    std::unordered_map<std::uint64_t, std::uint32_t> _talentByKey;
    std::unordered_map<std::uint32_t, KeyHandlerEvent> _downHandlesByKey;
    std::unordered_map<std::uint32_t, KeyHandlerEvent> _upHandlesByKey;
    std::unordered_map<std::uint32_t, ActiveAction> _activeActionsByKey;
};
