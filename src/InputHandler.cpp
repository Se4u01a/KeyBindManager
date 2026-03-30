#include "InputHandler.h"

#include "TalentBinder.h"
#include "UIManager.h"

#include <keyhandler/keyhandler.h>

namespace InputHandler
{
    namespace
    {
        KeyHandlerEvent g_menuToggleHandle = INVALID_REGISTRATION_HANDLE;
        constexpr std::uint32_t kEscapeKey = 0x01;  // Esc
    }

    void RefreshMenuHotkey()
    {
        auto* keyHandler = KeyHandler::GetSingleton();
        auto* talentBinder = TalentBinder::GetSingleton();
        if (!keyHandler || !talentBinder) {
            return;
        }

        if (g_menuToggleHandle != INVALID_REGISTRATION_HANDLE) {
            keyHandler->Unregister(g_menuToggleHandle);
            g_menuToggleHandle = INVALID_REGISTRATION_HANDLE;
        }

        const auto menuHotkey = talentBinder->GetMenuHotkey();
        const auto menuHotkeyLabel = talentBinder->GetMenuHotkeyLabel();
        logger::info("Registering menu toggle hotkey 0x{:X} ({}) and close hotkey 0x{:X} (Esc).",
            menuHotkey,
            menuHotkeyLabel,
            kEscapeKey);

        g_menuToggleHandle = keyHandler->Register(menuHotkey, KeyEventType::KEY_DOWN, []() {
            auto* uiManager = UIManager::GetSingleton();
            auto* talentBinder = TalentBinder::GetSingleton();
            logger::info("{} callback fired. UI ready={}, menuOpen={}",
                talentBinder->GetMenuHotkeyLabel(),
                uiManager->IsReady(),
                uiManager->IsMenuOpen());
            uiManager->ToggleFocus();
        });
    }

    void SetupInput()
    {
        KeyHandler::RegisterSink();
        TalentBinder::GetSingleton()->Initialize();
        logger::info("InputHandler setup started.");

        if (auto* keyHandler = KeyHandler::GetSingleton()) {
            RefreshMenuHotkey();

            (void) keyHandler->Register(kEscapeKey, KeyEventType::KEY_DOWN, []() {
                logger::info("Esc callback fired. Requesting menu close.");
                UIManager::GetSingleton()->CloseMenu();
            });
        }
    }
}
