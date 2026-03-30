#include "TalentBinder.h"

#include "InputHandler.h"
#include "KeyCodes.h"
#include "UIManager.h"

#include <Windows.h>

#ifdef GetObject
#    undef GetObject
#endif

#include <RE/A/ActorEquipManager.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/B/BGSDefaultObjectManager.h>
#include <RE/B/BGSEquipSlot.h>
#include <RE/B/BSInputEventQueue.h>
#include <RE/C/ControlMap.h>
#include <RE/C/CrosshairPickData.h>
#include <RE/T/TESShout.h>
#include <RE/U/UserEvents.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <unordered_set>

namespace
{
    constexpr auto* kBindingsSection = "Bindings";
    constexpr auto* kBindingLabelsSection = "BindingLabels";
    constexpr auto* kSettingsSection = "Settings";
    constexpr auto* kMenuKeyName = "MenuKey";
    constexpr auto* kModifierKeyName = "ModifierKey";
    constexpr auto* kPluginName = "KeyBindManager";
    constexpr std::uint32_t kEscapeKey = 0x01;
    constexpr std::uint32_t kDefaultMenuKey = 0x3D;
    constexpr std::uint32_t kDefaultModifierKey = 0x2A;

    std::optional<std::uint32_t> NormalizeModifierKey(const std::uint32_t dxScanCode)
    {
        switch (dxScanCode) {
        case 0x2A:
        case 0x36:
            return 0x2A;
        case 0x1D:
        case 0x9D:
            return 0x1D;
        case 0x38:
        case 0xB8:
            return 0x38;
        default:
            return std::nullopt;
        }
    }

    bool IsModifierKey(const std::uint32_t dxScanCode)
    {
        return NormalizeModifierKey(dxScanCode).has_value();
    }

    std::string GetModifierDisplayName(const std::uint32_t dxScanCode)
    {
        switch (NormalizeModifierKey(dxScanCode).value_or(0)) {
        case 0x2A:
            return "Shift";
        case 0x1D:
            return "Ctrl";
        case 0x38:
            return "Alt";
        default:
            return KeyCodes::DxScanCodeToDisplayName(dxScanCode);
        }
    }

    bool IsTalentSpellType(const RE::MagicSystem::SpellType spellType)
    {
        return spellType == RE::MagicSystem::SpellType::kPower ||
               spellType == RE::MagicSystem::SpellType::kLesserPower ||
               spellType == RE::MagicSystem::SpellType::kVoicePower;
    }

    std::string FormatFormIDLocal(const std::uint32_t formID)
    {
        std::ostringstream stream;
        stream << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << formID;
        return stream.str();
    }

    std::string GetTalentTypeLabel(const RE::MagicSystem::SpellType spellType, const bool raceTalent)
    {
        switch (spellType) {
        case RE::MagicSystem::SpellType::kLesserPower:
            return raceTalent ? "Race Lesser Power" : "Lesser Power";
        case RE::MagicSystem::SpellType::kVoicePower:
            return raceTalent ? "Race Voice Power" : "Voice Power";
        case RE::MagicSystem::SpellType::kPower:
        default:
            return raceTalent ? "Race Power" : "Power";
        }
    }

    std::filesystem::path GetCurrentModulePath()
    {
        HMODULE moduleHandle = nullptr;
        if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(&GetCurrentModulePath),
                &moduleHandle)) {
            return {};
        }

        std::array<char, MAX_PATH> buffer{};
        const auto length = GetModuleFileNameA(moduleHandle, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0 || length >= buffer.size()) {
            return {};
        }

        return std::filesystem::path(std::string(buffer.data(), length));
    }

    RE::TESObjectREFR* ResolveCrosshairTarget()
    {
        auto* crosshair = RE::CrosshairPickData::GetSingleton();
        if (!crosshair) {
            return nullptr;
        }

#if defined(EXCLUSIVE_SKYRIM_FLAT)
        if (auto targetActor = crosshair->targetActor.get()) {
            return targetActor.get();
        }

        if (auto target = crosshair->target.get()) {
            return target.get();
        }
#else
        if (auto targetActor = crosshair->targetActor[0].get()) {
            return targetActor.get();
        }

        if (auto target = crosshair->target[0].get()) {
            return target.get();
        }
#endif

        return nullptr;
    }

}

TalentBinder* TalentBinder::GetSingleton()
{
    static TalentBinder singleton;
    return &singleton;
}

void TalentBinder::Initialize()
{
    std::scoped_lock lock(_mutex);
    if (_initialized) {
        return;
    }

    _initialized = true;
    LoadBindings();
    KeyHandler::GetSingleton()->SetRawInputObserver([this](const std::uint32_t dxScanCode, const KeyEventType eventType) {
        return HandleCapturedInput(dxScanCode, eventType);
    });
}

void TalentBinder::RefreshMenu()
{
    PushSync();
}

std::uint32_t TalentBinder::GetMenuHotkey() const
{
    std::scoped_lock lock(_mutex);
    return _menuHotkey;
}

std::string TalentBinder::GetMenuHotkeyLabel() const
{
    std::scoped_lock lock(_mutex);
    return KeyCodes::DxScanCodeToDisplayName(_menuHotkey);
}

bool TalentBinder::IsMenuHotkey(const std::uint32_t dxScanCode) const
{
    std::scoped_lock lock(_mutex);
    return dxScanCode == _menuHotkey;
}

void TalentBinder::HandleUIMessage(const std::string& message)
{
    constexpr std::string_view unbindPrefix = "unbind:";
    constexpr std::string_view activatePrefix = "activate:";
    constexpr std::string_view listenBindPrefix = "listen_bind:";
    constexpr std::string_view listenMenuKeyCommand = "listen_menu_key";
    constexpr std::string_view resetMenuKeyCommand = "reset_menu_key";
    constexpr std::string_view closeMenuCommand = "close_menu";
    if (message == closeMenuCommand) {
        ClearPendingCapture();
        UIManager::GetSingleton()->CloseMenu();
        return;
    }

    if (message == listenMenuKeyCommand) {
        BeginMenuKeyCapture();
        return;
    }

    if (message == resetMenuKeyCommand) {
        ClearPendingCapture();
        if (HasMenuHotkeyConflict(kDefaultMenuKey)) {
            PushSync("Стандартная клавиша F3 уже используется одним из биндов талантов.");
            return;
        }
        SetMenuHotkey(kDefaultMenuKey);
        PushSync("Клавиша меню сброшена на F3.");
        return;
    }

    if (message.starts_with(listenBindPrefix)) {
        if (const auto formID = ParseFormID(std::string_view(message).substr(listenBindPrefix.size()))) {
            BeginBindingCapture(*formID);
        } else {
            PushSync("Could not start bind capture.");
        }
        return;
    }

    if (message.starts_with(unbindPrefix)) {
        ClearPendingCapture();
        if (const auto formID = ParseFormID(std::string_view(message).substr(unbindPrefix.size()))) {
            ClearBinding(*formID);
            PushSync("Hotkey cleared.");
        } else {
            PushSync("Could not clear that hotkey.");
        }
        return;
    }

    if (message.starts_with(activatePrefix)) {
        if (const auto formID = ParseFormID(std::string_view(message).substr(activatePrefix.size()))) {
            PushSync(ActivateTalent(*formID) ? "Item activated." : "Could not activate that item.");
        } else {
            PushSync("Could not activate that item.");
        }
    }
}

void TalentBinder::BeginBindingCapture(const std::uint32_t formID)
{
    std::scoped_lock lock(_mutex);
    _pendingMenuKeyCapture = false;
    _pendingTalentBinding = formID;
    _pendingModifierBinding.reset();
    SKSE::log::info("Waiting for next input to bind talent {}.", FormatFormID(formID));
}

void TalentBinder::BeginMenuKeyCapture()
{
    std::scoped_lock lock(_mutex);
    _pendingMenuKeyCapture = true;
    _pendingTalentBinding.reset();
    _pendingModifierBinding.reset();
    SKSE::log::info("Waiting for next input to rebind the menu hotkey.");
}

void TalentBinder::ClearPendingCapture()
{
    std::scoped_lock lock(_mutex);
    _pendingMenuKeyCapture = false;
    _pendingTalentBinding.reset();
    _pendingModifierBinding.reset();
}

bool TalentBinder::HandleCapturedInput(const std::uint32_t dxScanCode, const KeyEventType eventType)
{
    enum class CaptureAction : std::uint8_t
    {
        kNone,
        kCancel,
        kSetMenuKey,
        kSetTalentBinding
    };

    CaptureAction action = CaptureAction::kNone;
    std::optional<std::uint32_t> pendingTalentBinding;
    std::uint32_t bindingModifierDxScanCode = 0;

    if (const auto normalizedModifier = NormalizeModifierKey(dxScanCode)) {
        std::scoped_lock lock(_mutex);
        if (eventType == KeyEventType::KEY_DOWN) {
            _heldModifierKeys.insert(*normalizedModifier);
        } else if (eventType == KeyEventType::KEY_UP) {
            _heldModifierKeys.erase(*normalizedModifier);
        }
    }

    {
        std::scoped_lock lock(_mutex);
        if (!_pendingMenuKeyCapture && !_pendingTalentBinding.has_value()) {
            return false;
        }

        if (eventType == KeyEventType::KEY_UP) {
            return true;
        }

        if (dxScanCode == kEscapeKey) {
            _pendingMenuKeyCapture = false;
            _pendingTalentBinding.reset();
            _pendingModifierBinding.reset();
            action = CaptureAction::kCancel;
        } else if (_pendingMenuKeyCapture) {
            _pendingMenuKeyCapture = false;
            action = CaptureAction::kSetMenuKey;
        } else if (const auto normalizedModifier = NormalizeModifierKey(dxScanCode)) {
            _pendingModifierBinding = *normalizedModifier;
            SKSE::log::info("Modifier {} selected for pending binding.", GetModifierDisplayName(*normalizedModifier));
            return true;
        } else {
            pendingTalentBinding = _pendingTalentBinding;
            _pendingTalentBinding.reset();
            bindingModifierDxScanCode = _pendingModifierBinding.value_or(0);
            _pendingModifierBinding.reset();
            action = CaptureAction::kSetTalentBinding;
        }
    }

    switch (action) {
    case CaptureAction::kCancel:
        PushSync("Binding cancelled.");
        return true;
    case CaptureAction::kSetMenuKey:
        if (IsForbiddenMenuHotkey(dxScanCode)) {
            PushSync(GetMenuHotkeyConflictReason(dxScanCode));
            return true;
        }
        if (HasMenuHotkeyConflict(dxScanCode)) {
            PushSync(GetMenuHotkeyConflictReason(dxScanCode));
            return true;
        }
        SetMenuHotkey(dxScanCode);
        PushSync("Клавиша меню сохранена.");
        return true;
    case CaptureAction::kSetTalentBinding:
        if (!pendingTalentBinding.has_value()) {
            PushSync("Could not save that hotkey.");
            return true;
        }
        if (IsMenuHotkey(dxScanCode) || IsModifierKey(dxScanCode)) {
            PushSync("This key is reserved.");
            return true;
        }
        SetBinding(*pendingTalentBinding, dxScanCode, bindingModifierDxScanCode, KeyCodes::DxScanCodeToDisplayName(dxScanCode));
        PushSync("Hotkey saved.");
        return true;
    case CaptureAction::kNone:
    default:
        return false;
    }
}

std::filesystem::path TalentBinder::GetConfigPath() const
{
    auto modulePath = GetCurrentModulePath();
    if (modulePath.empty()) {
        return std::filesystem::path(kPluginName).replace_extension(".ini");
    }

    return modulePath.parent_path() / (std::string(kPluginName) + ".ini");
}

std::vector<TalentBinder::TalentEntry> TalentBinder::CollectTalents() const
{
    std::vector<TalentEntry> entries;

    const auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        return entries;
    }

    std::unordered_map<std::uint32_t, bool> seen;

    const auto addTalentSpell = [&](RE::SpellItem* spell, const bool raceTalent) {
        if (!spell || !IsTalentSpellType(spell->GetSpellType())) {
            return;
        }

        const auto formID = spell->GetFormID();
        if (seen.contains(formID)) {
            return;
        }
        seen[formID] = true;

        auto name = std::string(spell->GetFullName());
        if (name.empty()) {
            name = "Talent " + FormatFormIDLocal(formID);
        }

        entries.push_back({ formID, std::move(name), "Talents", GetTalentTypeLabel(spell->GetSpellType(), raceTalent) });
    };

    for (auto* spell : player->GetActorRuntimeData().addedSpells) {
        addTalentSpell(spell, false);
    }

    if (const auto* actorBase = player->GetActorBase(); actorBase && actorBase->actorEffects) {
        for (std::uint32_t index = 0; index < actorBase->actorEffects->numSpells; ++index) {
            addTalentSpell(actorBase->actorEffects->spells[index], false);
        }

    }

    if (const auto* race = player->GetRace(); race && race->actorEffects) {
        for (std::uint32_t index = 0; index < race->actorEffects->numSpells; ++index) {
            addTalentSpell(race->actorEffects->spells[index], true);
        }

    }

    std::sort(entries.begin(), entries.end(), [](const TalentEntry& left, const TalentEntry& right) {
        static const std::array<std::string_view, 1> order{ "Talents" };

        if (left.categoryLabel != right.categoryLabel) {
            const auto leftIt = std::find(order.begin(), order.end(), left.categoryLabel);
            const auto rightIt = std::find(order.begin(), order.end(), right.categoryLabel);
            return leftIt < rightIt;
        }

        return left.name < right.name;
    });

    return entries;
}

std::string TalentBinder::BuildSyncPayload(const std::string& statusMessage) const
{
    const auto entries = CollectTalents();

    std::ostringstream stream;
    stream << "{\"type\":\"sync\",\"menuKey\":\"" << EscapeJson(GetMenuHotkeyLabel()) << "\",\"status\":\"" << EscapeJson(statusMessage)
           << "\",\"talents\":[";

    bool isFirst = true;
    for (const auto& entry : entries) {
        if (!isFirst) {
            stream << ',';
        }
        isFirst = false;

        std::uint32_t boundKey = 0;
        std::uint32_t modifierDxScanCode = 0;
        std::string bindingLabel;
        {
            std::scoped_lock lock(_mutex);
            if (const auto binding = _bindingsByTalent.find(entry.formID); binding != _bindingsByTalent.end()) {
                boundKey = binding->second.dxScanCode;
                modifierDxScanCode = binding->second.modifierDxScanCode;
                bindingLabel = binding->second.displayLabel;
            }
        }

        if (bindingLabel.empty()) {
            bindingLabel = KeyCodes::DxScanCodeToDisplayName(boundKey);
        }
        if (modifierDxScanCode != 0 && !bindingLabel.empty()) {
            bindingLabel = GetModifierDisplayName(modifierDxScanCode) + " + " + bindingLabel;
        }

        stream << "{"
               << "\"id\":\"" << FormatFormID(entry.formID) << "\","
               << "\"name\":\"" << EscapeJson(entry.name) << "\","
               << "\"categoryLabel\":\"" << EscapeJson(entry.categoryLabel) << "\","
               << "\"typeLabel\":\"" << EscapeJson(entry.typeLabel) << "\","
               << "\"bindingLabel\":\"" << EscapeJson(bindingLabel) << "\""
               << "}";
    }

    stream << "]}";
    return stream.str();
}

bool TalentBinder::ActivateTalent(const std::uint32_t formID) const
{
    if (auto* shout = RE::TESForm::LookupByID<RE::TESShout>(formID)) {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* equipManager = RE::ActorEquipManager::GetSingleton();
        if (!player || !equipManager || !player->HasShout(shout)) {
            return false;
        }

        equipManager->EquipShout(player, shout);
        return true;
    }

    if (auto* spell = LookupTalent(formID)) {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* equipManager = RE::ActorEquipManager::GetSingleton();
        if (!player || !equipManager) {
            return false;
        }

        equipManager->EquipSpell(player, spell);
        if (auto* caster = player->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant)) {
            caster->CastSpellImmediate(spell, false, ResolveCrosshairTarget(), 1.0F, false, 0.0F, player);
            return true;
        }
    }

    return false;
}

bool TalentBinder::TriggerShout(RE::TESShout* shout, const std::uint32_t triggerDxScanCode) const
{
    (void) triggerDxScanCode;

    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player || !shout || !player->HasShout(shout)) {
        return false;
    }

    auto* equipManager = RE::ActorEquipManager::GetSingleton();
    if (!equipManager) {
        return false;
    }

    equipManager->EquipShout(player, shout);
    return true;
}

RE::SpellItem* TalentBinder::LookupTalent(const std::uint32_t formID) const
{
    return RE::TESForm::LookupByID<RE::SpellItem>(formID);
}

void TalentBinder::LoadBindings()
{
    _bindingsByTalent.clear();
    _talentByKey.clear();
    _activeActionsByKey.clear();
    _heldModifierKeys.clear();
    _pendingMenuKeyCapture = false;
    _pendingTalentBinding.reset();
    _pendingModifierBinding.reset();

    const auto iniPath = GetConfigPath().string();
    {
        char menuKeyBuffer[64]{};
        GetPrivateProfileStringA(kSettingsSection, kMenuKeyName, "61", menuKeyBuffer, static_cast<DWORD>(std::size(menuKeyBuffer)), iniPath.c_str());
        const std::string_view menuKeyText(menuKeyBuffer);
        const auto [ptr, ec] = std::from_chars(menuKeyText.data(), menuKeyText.data() + menuKeyText.size(), _menuHotkey);
        if (ec != std::errc{} || ptr != menuKeyText.data() + menuKeyText.size() || IsForbiddenMenuHotkey(_menuHotkey)) {
            _menuHotkey = kDefaultMenuKey;
        }
    }

    std::uint32_t legacyModifierKey = kDefaultModifierKey;
    {
        char modifierBuffer[64]{};
        GetPrivateProfileStringA(kSettingsSection, kModifierKeyName, "42", modifierBuffer, static_cast<DWORD>(std::size(modifierBuffer)), iniPath.c_str());
        const std::string_view modifierText(modifierBuffer);
        const auto [ptr, ec] = std::from_chars(modifierText.data(), modifierText.data() + modifierText.size(), legacyModifierKey);
        if (ec != std::errc{} || ptr != modifierText.data() + modifierText.size()) {
            legacyModifierKey = kDefaultModifierKey;
        }
        legacyModifierKey = NormalizeModifierKey(legacyModifierKey).value_or(kDefaultModifierKey);
    }

    std::vector<char> buffer(8192, '\0');
    const auto copied = GetPrivateProfileSectionA(kBindingsSection, buffer.data(), static_cast<DWORD>(buffer.size()), iniPath.c_str());

    if (copied != 0) {
        for (const char* cursor = buffer.data(); *cursor != '\0'; cursor += std::strlen(cursor) + 1) {
            const std::string_view entry(cursor);
            const auto separator = entry.find('=');
            if (separator == std::string_view::npos) {
                continue;
            }

            const auto formID = ParseFormID(entry.substr(0, separator));
            const auto value = entry.substr(separator + 1);
            const auto valueSeparator = value.find(',');
            const auto dxText = valueSeparator == std::string_view::npos ? value : value.substr(0, valueSeparator);
            const auto modifierText = valueSeparator == std::string_view::npos ? std::string_view{} : value.substr(valueSeparator + 1);

            std::uint32_t dxScanCode = 0;
            const auto [ptr, ec] = std::from_chars(dxText.data(), dxText.data() + dxText.size(), dxScanCode);
            if (!formID || ec != std::errc{} || ptr != dxText.data() + dxText.size()) {
                continue;
            }

            std::uint32_t modifierDxScanCode = 0;
            if (!modifierText.empty()) {
                if (modifierText == "1") {
                    modifierDxScanCode = legacyModifierKey;
                } else if (modifierText != "0") {
                    std::uint32_t parsedModifierDxScanCode = 0;
                    const auto [modifierPtr, modifierEc] =
                        std::from_chars(modifierText.data(), modifierText.data() + modifierText.size(), parsedModifierDxScanCode);
                    if (modifierEc != std::errc{} || modifierPtr != modifierText.data() + modifierText.size()) {
                        continue;
                    }

                    modifierDxScanCode = NormalizeModifierKey(parsedModifierDxScanCode).value_or(0);
                    if (modifierDxScanCode == 0) {
                        continue;
                    }
                }
            }

            if (dxScanCode == _menuHotkey || IsModifierKey(dxScanCode)) {
                continue;
            }

            BindingInfo bindingInfo;
            bindingInfo.dxScanCode = dxScanCode;
            bindingInfo.modifierDxScanCode = modifierDxScanCode;

            std::array<char, 256> labelBuffer{};
            GetPrivateProfileStringA(kBindingLabelsSection,
                FormatFormID(*formID).c_str(),
                "",
                labelBuffer.data(),
                static_cast<DWORD>(labelBuffer.size()),
                iniPath.c_str());

            const auto label = KeyCodes::NormalizeDisplayName(labelBuffer.data());
            if (!label.empty()) {
                bindingInfo.displayLabel = label;
            }

            _bindingsByTalent[*formID] = std::move(bindingInfo);
            _talentByKey[MakeBindingKey(dxScanCode, modifierDxScanCode)] = *formID;
        }
    }

    if (HasMenuHotkeyConflictUnlocked(_menuHotkey)) {
        SKSE::log::warn("Configured menu key {} conflicts with an existing talent binding. Falling back to default F3.",
            KeyCodes::DxScanCodeToDisplayName(_menuHotkey));
        _menuHotkey = kDefaultMenuKey;
    }

    RebuildHotkeyRegistrations();
}

void TalentBinder::PersistBindings() const
{
    const auto iniPath = GetConfigPath().string();
    WritePrivateProfileStringA(kBindingsSection, nullptr, nullptr, iniPath.c_str());
    WritePrivateProfileStringA(kBindingLabelsSection, nullptr, nullptr, iniPath.c_str());
    WritePrivateProfileStringA(kSettingsSection, nullptr, nullptr, iniPath.c_str());
    WritePrivateProfileStringA(kSettingsSection, kMenuKeyName, std::to_string(_menuHotkey).c_str(), iniPath.c_str());

    for (const auto& [formID, binding] : _bindingsByTalent) {
        const auto key = FormatFormID(formID);
        const auto value = std::to_string(binding.dxScanCode) + "," + std::to_string(binding.modifierDxScanCode);
        WritePrivateProfileStringA(kBindingsSection, key.c_str(), value.c_str(), iniPath.c_str());

        const auto& label = binding.displayLabel.empty() ? KeyCodes::DxScanCodeToDisplayName(binding.dxScanCode) : binding.displayLabel;
        WritePrivateProfileStringA(kBindingLabelsSection, key.c_str(), label.c_str(), iniPath.c_str());
    }
}

void TalentBinder::SetMenuHotkey(const std::uint32_t dxScanCode)
{
    {
        std::scoped_lock lock(_mutex);
        _menuHotkey = dxScanCode;
        PersistBindings();
    }

    InputHandler::RefreshMenuHotkey();
}

bool TalentBinder::IsForbiddenMenuHotkey(const std::uint32_t dxScanCode) const
{
    return dxScanCode == kEscapeKey || IsModifierKey(dxScanCode);
}

bool TalentBinder::HasMenuHotkeyConflict(const std::uint32_t dxScanCode) const
{
    std::scoped_lock lock(_mutex);
    return HasMenuHotkeyConflictUnlocked(dxScanCode);
}

bool TalentBinder::HasMenuHotkeyConflictUnlocked(const std::uint32_t dxScanCode) const
{
    for (const auto& [formID, binding] : _bindingsByTalent) {
        (void) formID;
        if (binding.dxScanCode == dxScanCode) {
            return true;
        }
    }

    return false;
}

std::string TalentBinder::GetMenuHotkeyConflictReason(const std::uint32_t dxScanCode) const
{
    if (dxScanCode == kEscapeKey) {
        return "Esc зарезервирован для закрытия меню.";
    }

    if (IsModifierKey(dxScanCode)) {
        return "Shift, Ctrl и Alt нельзя использовать как клавишу меню.";
    }

    if (HasMenuHotkeyConflict(dxScanCode)) {
        return "Эта клавиша уже занята одним из биндов талантов.";
    }

    return "Эту клавишу нельзя использовать для меню.";
}

void TalentBinder::RebuildHotkeyRegistrations()
{
    auto* keyHandler = KeyHandler::GetSingleton();
    if (!keyHandler) {
        return;
    }

    for (const auto& [dxScanCode, handle] : _downHandlesByKey) {
        (void) dxScanCode;
        keyHandler->Unregister(handle);
    }
    for (const auto& [dxScanCode, handle] : _upHandlesByKey) {
        (void) dxScanCode;
        keyHandler->Unregister(handle);
    }
    _downHandlesByKey.clear();
    _upHandlesByKey.clear();

    std::unordered_set<std::uint32_t> registeredKeys;
    for (const auto& [formID, binding] : _bindingsByTalent) {
        (void) formID;
        const auto dxScanCode = binding.dxScanCode;
        if (!registeredKeys.insert(dxScanCode).second) {
            continue;
        }

        const auto downHandle = keyHandler->Register(dxScanCode, KeyEventType::KEY_DOWN, [this, dxScanCode]() {
            OnBoundHotkeyPressed(dxScanCode);
        });
        const auto upHandle = keyHandler->Register(dxScanCode, KeyEventType::KEY_UP, [this, dxScanCode]() {
            OnBoundHotkeyReleased(dxScanCode);
        });

        if (downHandle != INVALID_REGISTRATION_HANDLE) {
            _downHandlesByKey[dxScanCode] = downHandle;
        }
        if (upHandle != INVALID_REGISTRATION_HANDLE) {
            _upHandlesByKey[dxScanCode] = upHandle;
        }
    }
}

void TalentBinder::SetBinding(const std::uint32_t formID, const std::uint32_t dxScanCode, const std::uint32_t modifierDxScanCode, std::string displayLabel)
{
    std::scoped_lock lock(_mutex);

    if (const auto oldBinding = _bindingsByTalent.find(formID); oldBinding != _bindingsByTalent.end()) {
        _talentByKey.erase(MakeBindingKey(oldBinding->second.dxScanCode, oldBinding->second.modifierDxScanCode));
        _bindingsByTalent.erase(oldBinding);
    }

    const auto bindingKey = MakeBindingKey(dxScanCode, modifierDxScanCode);
    if (const auto oldTalent = _talentByKey.find(bindingKey); oldTalent != _talentByKey.end()) {
        _bindingsByTalent.erase(oldTalent->second);
        _talentByKey.erase(oldTalent);
    }

    BindingInfo bindingInfo;
    bindingInfo.dxScanCode = dxScanCode;
    bindingInfo.modifierDxScanCode = modifierDxScanCode;
    bindingInfo.displayLabel = displayLabel.empty() ? KeyCodes::DxScanCodeToDisplayName(dxScanCode) : std::move(displayLabel);

    _bindingsByTalent[formID] = std::move(bindingInfo);
    _talentByKey[bindingKey] = formID;

    PersistBindings();
    RebuildHotkeyRegistrations();
}

void TalentBinder::ClearBinding(const std::uint32_t formID)
{
    std::scoped_lock lock(_mutex);

    if (const auto oldBinding = _bindingsByTalent.find(formID); oldBinding != _bindingsByTalent.end()) {
        _activeActionsByKey.erase(oldBinding->second.dxScanCode);
        _talentByKey.erase(MakeBindingKey(oldBinding->second.dxScanCode, oldBinding->second.modifierDxScanCode));
        _bindingsByTalent.erase(oldBinding);
    }

    PersistBindings();
    RebuildHotkeyRegistrations();
}

void TalentBinder::OnBoundHotkeyPressed(const std::uint32_t dxScanCode)
{
    if (UIManager::GetSingleton()->IsMenuOpen()) {
        return;
    }

    std::optional<std::uint32_t> formID;
    {
        std::scoped_lock lock(_mutex);
        static constexpr std::array<std::uint32_t, 3> modifierPriority{ 0x2A, 0x1D, 0x38 };

        for (const auto modifierDxScanCode : modifierPriority) {
            if (!_heldModifierKeys.contains(modifierDxScanCode)) {
                continue;
            }

            if (const auto binding = _talentByKey.find(MakeBindingKey(dxScanCode, modifierDxScanCode)); binding != _talentByKey.end()) {
                formID = binding->second;
                break;
            }
        }

        if (!formID && _heldModifierKeys.empty()) {
            if (const auto binding = _talentByKey.find(MakeBindingKey(dxScanCode, 0)); binding != _talentByKey.end()) {
                formID = binding->second;
            }
        }
    }

    if (!formID) {
        return;
    }

    if (auto* shout = RE::TESForm::LookupByID<RE::TESShout>(*formID)) {
        if (TriggerShout(shout, dxScanCode)) {
            std::scoped_lock lock(_mutex);
            _activeActionsByKey[dxScanCode] = { *formID, ActiveActionType::kShout };
        }
        return;
    }

    if (auto* spell = LookupTalent(*formID)) {
        if (IsTalentSpellType(spell->GetSpellType())) {
            (void) ActivateTalent(*formID);
        }
    }
}

void TalentBinder::OnBoundHotkeyReleased(const std::uint32_t dxScanCode)
{
    std::optional<ActiveAction> action;
    {
        std::scoped_lock lock(_mutex);
        if (const auto it = _activeActionsByKey.find(dxScanCode); it != _activeActionsByKey.end()) {
            action = it->second;
            _activeActionsByKey.erase(it);
        }
    }

    if (!action) {
        return;
    }

    (void) dxScanCode;
}

void TalentBinder::PushSync(const std::string& statusMessage) const
{
    if (auto* ui = UIManager::GetSingleton(); ui->IsReady()) {
        ui->SendPayload(BuildSyncPayload(statusMessage));
    }
}

std::optional<std::uint32_t> TalentBinder::ParseFormID(const std::string_view text)
{
    std::uint32_t formID = 0;
    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), formID, 16);
    if (ec != std::errc{} || ptr != text.data() + text.size()) {
        return std::nullopt;
    }

    return formID;
}

std::string TalentBinder::FormatFormID(const std::uint32_t formID)
{
    return FormatFormIDLocal(formID);
}

std::uint64_t TalentBinder::MakeBindingKey(const std::uint32_t dxScanCode, const std::uint32_t modifierDxScanCode)
{
    return static_cast<std::uint64_t>(dxScanCode) | (static_cast<std::uint64_t>(modifierDxScanCode) << 32);
}

std::string TalentBinder::EscapeJson(const std::string_view value)
{
    std::string escaped;
    escaped.reserve(value.size());

    for (const char ch : value) {
        switch (ch) {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped += ch;
            break;
        }
    }

    return escaped;
}
