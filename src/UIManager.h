#pragma once

#include "PrismaUI_API.h"

#include <string>

class UIManager
{
public:
    static UIManager* GetSingleton();

    void Initialize();
    void ToggleFocus();
    void CloseMenu();
    void SendPayload(const std::string& payload);

    [[nodiscard]] bool IsReady() const;
    [[nodiscard]] bool IsMenuOpen() const;

    PRISMA_UI_API::IVPrismaUI1* GetAPI();
    PrismaView GetView();

private:
    void LogAPILoadFailure() const;

    UIManager() = default;
    ~UIManager() = default;
    UIManager(const UIManager&) = delete;
    UIManager& operator=(const UIManager&) = delete;

    PRISMA_UI_API::IVPrismaUI1* _prismaAPI = nullptr;
    PrismaView _mainView{};
};
