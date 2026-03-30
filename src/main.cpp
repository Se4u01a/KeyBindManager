#include "InputHandler.h"
#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include "UIManager.h"

void SKSEMessageHandler(SKSE::MessagingInterface::Message* message)
{
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        SKSE::log::info("Received SKSE kDataLoaded message, initializing input and deferring PrismaUI until first use.");
        InputHandler::SetupInput();
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    SKSE::Init(skse);
    SKSE::log::info("KeyBindManager plugin load started.");

    auto* messaging = SKSE::GetMessagingInterface();
    messaging->RegisterListener(SKSEMessageHandler);
    SKSE::log::info("Registered SKSE message listener.");

    return true;
}
