#include "UIManager.h"

#include "SKSE/SKSE.h"
#include "TalentBinder.h"

#include <Windows.h>

UIManager* UIManager::GetSingleton()
{
    static UIManager singleton;
    return &singleton;
}

void UIManager::Initialize()
{
    if (_prismaAPI && _mainView) {
        SKSE::log::info("UIManager::Initialize skipped because the PrismaUI view is already ready.");
        return;
    }

    _prismaAPI = static_cast<PRISMA_UI_API::IVPrismaUI1*>(
        PRISMA_UI_API::RequestPluginAPI(PRISMA_UI_API::InterfaceVersion::V1));

    if (!_prismaAPI) {
        LogAPILoadFailure();
        return;
    }

    _mainView = _prismaAPI->CreateView("KeyBindManager/index.html", [](PrismaView view) {
        SKSE::log::info("View DOM is ready {}", view);
        TalentBinder::GetSingleton()->RefreshMenu();
    });

    if (!_mainView) {
        SKSE::log::error("CreateView failed for Prisma view path KeyBindManager/index.html.");
        return;
    }

    _prismaAPI->RegisterJSListener(_mainView, "sendDataToSKSE", [](const char* data) {
        if (!data) {
            return;
        }

        SKSE::log::info("Received data from JS: {}", data);
        TalentBinder::GetSingleton()->HandleUIMessage(data);
    });

    _prismaAPI->Hide(_mainView);
    SKSE::log::info("UIManager successfully initialized.");
}

void UIManager::LogAPILoadFailure() const
{
    const HMODULE prismaModule = ::GetModuleHandleA("PrismaUI.dll");
    if (!prismaModule) {
        SKSE::log::error("Failed to load PrismaUI API because PrismaUI.dll is not loaded into the game process.");
        return;
    }

    const FARPROC requestAPI = ::GetProcAddress(prismaModule, "RequestPluginAPI");
    if (!requestAPI) {
        SKSE::log::error("Failed to load PrismaUI API because PrismaUI.dll does not export RequestPluginAPI.");
        return;
    }

    SKSE::log::error("Failed to load PrismaUI API even though PrismaUI.dll and RequestPluginAPI were found.");
}

void UIManager::ToggleFocus()
{
    if (!IsReady()) {
        SKSE::log::info("ToggleFocus requested before PrismaUI was ready, attempting lazy initialization.");
        Initialize();
    }

    if (!_prismaAPI || !_mainView) {
        SKSE::log::warn("ToggleFocus ignored because UI is not ready. prismaAPI={}, mainView={}", _prismaAPI != nullptr, _mainView);
        return;
    }

    const bool isHidden = _prismaAPI->IsHidden(_mainView);
    const bool hasFocus = _prismaAPI->HasFocus(_mainView);
    SKSE::log::info("ToggleFocus called. isHidden={}, hasFocus={}, view={}", isHidden, hasFocus, _mainView);

    if (isHidden || !hasFocus) {
        _prismaAPI->Show(_mainView);
        const bool focusResult = _prismaAPI->Focus(_mainView);
        SKSE::log::info("PrismaUI Focus returned {}", focusResult);
        if (focusResult) {
            TalentBinder::GetSingleton()->RefreshMenu();
        }
    } else {
        SKSE::log::info("Menu already visible and focused, closing it.");
        CloseMenu();
    }
}

void UIManager::CloseMenu()
{
    if (!_prismaAPI || !_mainView || _prismaAPI->IsHidden(_mainView)) {
        return;
    }

    if (_prismaAPI->HasFocus(_mainView)) {
        SKSE::log::info("Unfocusing Prisma view {}", _mainView);
        _prismaAPI->Unfocus(_mainView);
    }

    SKSE::log::info("Hiding Prisma view {}", _mainView);
    _prismaAPI->Hide(_mainView);
}

void UIManager::SendPayload(const std::string& payload)
{
    if (!_prismaAPI || !_mainView) {
        return;
    }

    _prismaAPI->InteropCall(_mainView, "handlePluginPayload", payload.c_str());
}

bool UIManager::IsReady() const
{
    return _prismaAPI != nullptr && _mainView != 0;
}

bool UIManager::IsMenuOpen() const
{
    return IsReady() && !_prismaAPI->IsHidden(_mainView);
}

PRISMA_UI_API::IVPrismaUI1* UIManager::GetAPI()
{
    return _prismaAPI;
}

PrismaView UIManager::GetView()
{
    return _mainView;
}
