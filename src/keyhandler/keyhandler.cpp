#include "keyhandler.h"

#include <Windows.h>

#include <chrono>
#include <optional>

namespace
{
    constexpr std::uint32_t MOUSE3_DX = 0x100;
    constexpr std::uint32_t MOUSE4_DX = 0x101;
    constexpr std::uint32_t MOUSE5_DX = 0x102;

    std::optional<std::uint32_t> TranslateInputCode(const RE::ButtonEvent* buttonEvent)
    {
        if (!buttonEvent) {
            return std::nullopt;
        }

        if (buttonEvent->GetDevice() == RE::INPUT_DEVICE::kKeyboard) {
            switch (buttonEvent->GetIDCode()) {
            case 0xEA:
                return MOUSE4_DX;
            case 0xE9:
                return MOUSE5_DX;
            default:
                return buttonEvent->GetIDCode();
            }
        }

        if (buttonEvent->GetDevice() != RE::INPUT_DEVICE::kMouse) {
            return std::nullopt;
        }

        switch (buttonEvent->GetIDCode()) {
        case 2:
            return MOUSE3_DX;
        case 3:
        case 5:
        case 7:
            return MOUSE4_DX;
        case 4:
        case 6:
        case 8:
            return MOUSE5_DX;
        default:
            return std::nullopt;
        }
    }
}

KeyHandler* KeyHandler::GetSingleton()
{
    static KeyHandler singleton;
    return &singleton;
}

KeyHandler::~KeyHandler() = default;

void KeyHandler::RegisterSink()
{
    auto inputMgr = RE::BSInputDeviceManager::GetSingleton();
    if (inputMgr) {
        inputMgr->AddEventSink(GetSingleton());
        GetSingleton()->StartMousePolling();
        logger::info("KeyHandler sink registered successfully.");
    }
    else {
        logger::critical("Failed to get InputDeviceManager. KeyHandler sink NOT registered!");
    }
}

void KeyHandler::StartMousePolling()
{
    bool expected = false;
    if (!_mousePollingStarted.compare_exchange_strong(expected, true)) {
        return;
    }

    _mousePollThread = std::jthread([this](std::stop_token stopToken) {
        bool mouse4Down = false;
        bool mouse5Down = false;

        while (!stopToken.stop_requested()) {
            const bool nextMouse4Down = (GetAsyncKeyState(VK_XBUTTON1) & 0x8000) != 0;
            const bool nextMouse5Down = (GetAsyncKeyState(VK_XBUTTON2) & 0x8000) != 0;

            if (nextMouse4Down != mouse4Down) {
                DispatchPolledKey(MOUSE4_DX, nextMouse4Down ? KeyEventType::KEY_DOWN : KeyEventType::KEY_UP);
                mouse4Down = nextMouse4Down;
            }

            if (nextMouse5Down != mouse5Down) {
                DispatchPolledKey(MOUSE5_DX, nextMouse5Down ? KeyEventType::KEY_DOWN : KeyEventType::KEY_UP);
                mouse5Down = nextMouse5Down;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });
}

void KeyHandler::DispatchPolledKey(const std::uint32_t dxScanCode, const KeyEventType eventType)
{
    std::vector<KeyCallback> callbacksToRun;

    {
        std::shared_lock lock(_mutex);

        auto keyCallbacksIt = _registeredCallbacks.find(dxScanCode);
        if (keyCallbacksIt == _registeredCallbacks.end()) {
            return;
        }

        const auto& keyCallbacks = keyCallbacksIt->second;
        const auto& targetMap = (eventType == KeyEventType::KEY_DOWN) ? keyCallbacks.down : keyCallbacks.up;

        callbacksToRun.reserve(targetMap.size());
        for (const auto& pair : targetMap) {
            callbacksToRun.push_back(pair.second);
        }
    }

    for (const auto& callback : callbacksToRun) {
        callback();
    }
}

void KeyHandler::SetRawInputObserver(RawInputObserver observer)
{
    std::unique_lock lock(_mutex);
    _rawInputObserver = std::move(observer);
}

[[nodiscard]] KeyHandlerEvent KeyHandler::Register(uint32_t dxScanCode, KeyEventType eventType, KeyCallback callback)
{
    if (!callback) {
        logger::warn("Attempted to register a null callback for key 0x{:X}", dxScanCode);
        return INVALID_REGISTRATION_HANDLE;
    }

    const KeyHandlerEvent handle = _nextHandle.fetch_add(1);
    if (handle == INVALID_REGISTRATION_HANDLE) {
        logger::critical("KeyHandlerEvent overflow detected!");
        _nextHandle.store(INVALID_REGISTRATION_HANDLE + 1);
        return INVALID_REGISTRATION_HANDLE;
    }

    std::unique_lock lock(_mutex);

    logger::info("Registering callback with handle {} for key 0x{:X}, event type {}", handle, dxScanCode, (eventType == KeyEventType::KEY_DOWN ? "DOWN" : "UP"));

    auto& keyCallbacks = _registeredCallbacks[dxScanCode];
    auto& targetMap = (eventType == KeyEventType::KEY_DOWN) ? keyCallbacks.down : keyCallbacks.up;
    targetMap[handle] = std::move(callback);

    _handleMap[handle] = { dxScanCode, eventType };

    return handle;
}

void KeyHandler::Unregister(KeyHandlerEvent handle)
{
    if (handle == INVALID_REGISTRATION_HANDLE) {
        logger::warn("Attempted to unregister with an invalid handle.");
        return;
    }

    CallbackInfo info;
    bool foundHandle = false;

    std::unique_lock lock(_mutex);

    auto handleIt = _handleMap.find(handle);
    if (handleIt != _handleMap.end()) {
        info = handleIt->second;
        foundHandle = true;
        _handleMap.erase(handleIt);
    }
    else {
        logger::warn("Attempted to unregister handle {}, but it was not found. It might have been already unregistered.", handle);
        return;
    }

    auto keyCallbacksIt = _registeredCallbacks.find(info.key);
    if (keyCallbacksIt != _registeredCallbacks.end()) {
        auto& keyCallbacks = keyCallbacksIt->second;
        auto& targetMap = (info.type == KeyEventType::KEY_DOWN) ? keyCallbacks.down : keyCallbacks.up;

        size_t removedCount = targetMap.erase(handle);

        if (removedCount > 0) {
            logger::info("Unregistered callback with handle {} for key 0x{:X}, event type {}", handle, info.key, (info.type == KeyEventType::KEY_DOWN ? "DOWN" : "UP"));
        }
        else {
            logger::error("Inconsistency detected: Handle {} found in handle map but corresponding callback not found for key 0x{:X}.", handle, info.key);
        }

        if (keyCallbacks.down.empty() && keyCallbacks.up.empty()) {
            logger::debug("Removing empty key entry 0x{:X} from callback map.", info.key);
            _registeredCallbacks.erase(keyCallbacksIt);
        }

    }
    else {
        logger::error("Inconsistency detected: Handle {} found in handle map but key 0x{:X} not found in callback map.", handle, info.key);
    }
}


RE::BSEventNotifyControl KeyHandler::ProcessEvent(RE::InputEvent* const* a_eventList, [[maybe_unused]] RE::BSTEventSource<RE::InputEvent*>* a_eventSource)
{
    if (!a_eventList) {
        return RE::BSEventNotifyControl::kContinue;
    }

    std::vector<KeyCallback> callbacksToRun;

    for (auto event = *a_eventList; event; event = event->next) {
        if (event->eventType != RE::INPUT_EVENT_TYPE::kButton) {
            continue;
        }

        const auto buttonEvent = event->AsButtonEvent();
        if (!buttonEvent) {
            continue;
        }

        const auto translatedCode = TranslateInputCode(buttonEvent);
        if (!translatedCode) {
            continue;
        }

        const uint32_t dxScanCode = *translatedCode;
        KeyEventType eventType;

        if (buttonEvent->IsDown()) {
            eventType = KeyEventType::KEY_DOWN;
        }
        else if (buttonEvent->IsUp()) {
            eventType = KeyEventType::KEY_UP;
        }
        else {
            continue;
        }

        RawInputObserver rawObserver;
        {
            std::shared_lock lock(_mutex);
            rawObserver = _rawInputObserver;
        }

        if (rawObserver && rawObserver(dxScanCode, eventType)) {
            continue;
        }

        {
            std::shared_lock lock(_mutex);

            auto keyCallbacksIt = _registeredCallbacks.find(dxScanCode);
            if (keyCallbacksIt != _registeredCallbacks.end()) {
                const auto& keyCallbacks = keyCallbacksIt->second;
                const auto& targetMap = (eventType == KeyEventType::KEY_DOWN) ? keyCallbacks.down : keyCallbacks.up;

                if (!targetMap.empty()) {
                    callbacksToRun.reserve(callbacksToRun.size() + targetMap.size());
                    for (const auto& pair : targetMap) {
                        callbacksToRun.push_back(pair.second);
                    }
                }
            }
        }

    }

    if (!callbacksToRun.empty()) {
        logger::debug("Executing {} collected callbacks...", callbacksToRun.size());
        for (const auto& callback : callbacksToRun) {
            callback();
        }
    }

    return RE::BSEventNotifyControl::kContinue;
}
