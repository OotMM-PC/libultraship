#include "ship/window/Window.h"
#include "ship/window/WindowEmbedding.h"
#include <cstdlib>
#include <limits>
#include <string>
#include <fstream>
#include <iostream>
#include "ship/controller/controldevice/controller/mapping/keyboard/KeyboardScancodes.h"
#include "ship/Context.h"
#include "ship/controller/controldeck/ControlDeck.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#ifdef __APPLE__
#include "ship/utils/AppleFolderManager.h"
#endif

namespace Ship {

Window::Window(std::shared_ptr<Gui> gui, std::shared_ptr<MouseStateManager> mouseStateManager) {
    mGui = gui;
    mMouseStateManager = mouseStateManager;
    mAvailableWindowBackends = std::make_shared<std::vector<WindowBackend>>();
    mConfig = Context::GetInstance()->GetConfig();
}

Window::Window(std::shared_ptr<Gui> gui) : Window(gui, std::make_shared<MouseStateManager>()) {
}

Window::Window(std::vector<std::shared_ptr<GuiWindow>> guiWindows) : Window(std::make_shared<Gui>(guiWindows)) {
}

Window::Window() : Window(std::vector<std::shared_ptr<GuiWindow>>()) {
}

Window::~Window() {
    mGui->ShutDownImGui(this);
    SPDLOG_DEBUG("destruct window");
}

void Window::ToggleFullscreen() {
    SetFullscreen(!IsFullscreen());
}

float Window::GetCurrentAspectRatio() {
    return (float)GetWidth() / (float)GetHeight();
}

int32_t Window::GetLastScancode() {
    return mLastScancode;
}

void Window::SetLastScancode(int32_t scanCode) {
    mLastScancode = scanCode;
}

std::shared_ptr<Gui> Window::GetGui() {
    return mGui;
}

void Window::SaveWindowToConfig() {
    // This accepts conf in because it can be run in the destruction of LUS.
    mConfig->SetBool("Window.Fullscreen.Enabled", IsFullscreen());
    if (IsFullscreen()) {
        mConfig->SetInt("Window.Fullscreen.Width", (int32_t)GetWidth());
        mConfig->SetInt("Window.Fullscreen.Height", (int32_t)GetHeight());
    } else {
        mConfig->SetInt("Window.Width", (int32_t)GetWidth());
        mConfig->SetInt("Window.Height", (int32_t)GetHeight());
        mConfig->SetInt("Window.PositionX", GetPosX());
        mConfig->SetInt("Window.PositionY", GetPosY());
    }
}

WindowBackend Window::GetWindowBackend() {
    return mWindowBackend;
}

std::shared_ptr<std::vector<WindowBackend>> Window::GetAvailableWindowBackends() {
    return mAvailableWindowBackends;
}

bool Window::IsAvailableWindowBackend(int32_t backendId) {
    // Verify the id is a valid backend enum value
    if (backendId < 0 || backendId >= static_cast<int>(WindowBackend::WINDOW_BACKEND_COUNT)) {
        return false;
    }

    // Verify the backend is available
    auto backend = static_cast<WindowBackend>(backendId);
    return std::find(mAvailableWindowBackends->begin(), mAvailableWindowBackends->end(), backend) !=
           mAvailableWindowBackends->end();
}

bool Window::ShouldAutoCaptureMouse() {
    return mMouseStateManager->ShouldAutoCaptureMouse();
}

void Window::SetAutoCaptureMouse(bool capture) {
    mMouseStateManager->SetAutoCaptureMouse(capture);
}

bool Window::ShouldForceCursorVisibility() {
    return mMouseStateManager->ShouldForceCursorVisibility();
}

void Window::SetForceCursorVisibility(bool visible) {
    mMouseStateManager->SetForceCursorVisibility(visible);
}

int32_t Window::GetFullscreenScancode() {
    return mFullscreenScancode;
}

int32_t Window::GetMouseCaptureScancode() {
    return mMouseCaptureScancode;
}

void Window::SetFullscreenScancode(int32_t scancode) {
    mFullscreenScancode = scancode;
}

void Window::SetMouseCaptureScancode(int32_t scancode) {
    mMouseCaptureScancode = scancode;
}

std::shared_ptr<MouseStateManager> Window::GetMouseStateManager() {
    return mMouseStateManager;
}

void Window::SetWindowBackend(WindowBackend backend) {
    mWindowBackend = backend;
    Context::GetInstance()->GetConfig()->SetWindowBackend(GetWindowBackend());
    Context::GetInstance()->GetConfig()->Save();
}

void Window::AddAvailableWindowBackend(WindowBackend backend) {
    mAvailableWindowBackends->push_back(backend);
}

uintptr_t GetExternalWindowParent() {
#ifdef _WIN32
    static const uintptr_t parent = [] {
        const char* value = std::getenv("OOTMM_PARENT_HWND");
        if (value == nullptr || *value == '\0') {
            return uintptr_t{ 0 };
        }

        uintptr_t parsed = 0;
        for (const char* character = value; *character != '\0'; ++character) {
            if (*character < '0' || *character > '9') {
                return uintptr_t{ 0 };
            }

            const auto digit = static_cast<uintptr_t>(*character - '0');
            if (parsed > (std::numeric_limits<uintptr_t>::max() - digit) / 10) {
                return uintptr_t{ 0 };
            }
            parsed = parsed * 10 + digit;
        }
        return parsed;
    }();
    return parent;
#else
    return 0;
#endif
}

bool IsWindowEmbedded() {
    return GetExternalWindowParent() != 0;
}

bool IsExternalWindowParentAlive() {
#ifdef _WIN32
    const auto parent = reinterpret_cast<HWND>(GetExternalWindowParent());
    return parent == nullptr || IsWindow(parent);
#else
    return true;
#endif
}

} // namespace Ship
