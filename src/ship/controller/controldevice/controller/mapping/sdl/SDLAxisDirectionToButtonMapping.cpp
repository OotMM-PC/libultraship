#include "ship/controller/controldevice/controller/mapping/sdl/SDLAxisDirectionToButtonMapping.h"
#include <cmath>
#include <spdlog/spdlog.h>
#include "ship/utils/StringHelper.h"
#include "ship/window/gui/IconsFontAwesome4.h"
#include "ship/config/ConsoleVariable.h"
#include "ship/Context.h"
#include "ship/controller/controldeck/ControlDeck.h"
#include "ship/controller/controldevice/controller/Controller.h"

namespace Ship {
SDLAxisDirectionToButtonMapping::SDLAxisDirectionToButtonMapping(uint8_t portIndex, CONTROLLERBUTTONS_T bitmask,
                                                                 int32_t sdlControllerAxis, int32_t axisDirection)
    : ControllerInputMapping(PhysicalDeviceType::SDLGamepad),
      ControllerButtonMapping(PhysicalDeviceType::SDLGamepad, portIndex, bitmask),
      SDLAxisDirectionToAnyMapping(sdlControllerAxis, axisDirection) {
}

namespace {
constexpr float kLeanMargin = 1.2f;

SDL_GameControllerAxis OtherStickAxis(SDL_GameControllerAxis axis) {
    switch (axis) {
        case SDL_CONTROLLER_AXIS_LEFTX:
            return SDL_CONTROLLER_AXIS_LEFTY;
        case SDL_CONTROLLER_AXIS_LEFTY:
            return SDL_CONTROLLER_AXIS_LEFTX;
        case SDL_CONTROLLER_AXIS_RIGHTX:
            return SDL_CONTROLLER_AXIS_RIGHTY;
        default:
            return SDL_CONTROLLER_AXIS_RIGHTX;
    }
}
} // namespace

bool SDLAxisDirectionToButtonMapping::OtherAxisHasButton() const {
    const SDL_GameControllerAxis other = OtherStickAxis(mControllerAxis);
    const auto controller = Context::GetInstance()->GetControlDeck()->GetControllerByPort(mPortIndex);
    for (const auto& [bitmask, button] : controller->GetAllButtons()) {
        for (const auto& [id, mapping] : button->GetAllButtonMappings()) {
            const auto stick = std::dynamic_pointer_cast<SDLAxisDirectionToButtonMapping>(mapping);
            if (stick != nullptr && stick->mControllerAxis == other) {
                return true;
            }
        }
    }
    return false;
}

bool SDLAxisDirectionToButtonMapping::LeansThisWay(SDL_GameController* gamepad, int32_t axisValue) const {
    const float own = std::abs(static_cast<float>(axisValue));
    const float other = std::abs(static_cast<float>(SDL_GameControllerGetAxis(gamepad, OtherStickAxis(mControllerAxis))));
    return mLeaning ? own * kLeanMargin >= other : own > other * kLeanMargin;
}

void SDLAxisDirectionToButtonMapping::UpdatePad(CONTROLLERBUTTONS_T& padButtons) {
    if (Context::GetInstance()->GetControlDeck()->GamepadGameInputBlocked()) {
        mLeaning = false;
        return;
    }

    int32_t axisThresholdPercentage = 25;
    bool strongestDirectionOnly = false;
    if (AxisIsStick()) {
        const auto globalSettings = Ship::Context::GetInstance()->GetControlDeck()->GetGlobalSDLDeviceSettings();
        axisThresholdPercentage = globalSettings->GetStickAxisThresholdPercentage();
        strongestDirectionOnly = globalSettings->GetStickButtonsStrongestDirectionOnly() && OtherAxisHasButton();
    } else if (AxisIsTrigger()) {
        axisThresholdPercentage = Ship::Context::GetInstance()
                                      ->GetControlDeck()
                                      ->GetGlobalSDLDeviceSettings()
                                      ->GetTriggerAxisThresholdPercentage();
    }

    bool leaning = false;
    for (const auto& [instanceId, gamepad] :
         Context::GetInstance()->GetControlDeck()->GetConnectedPhysicalDeviceManager()->GetConnectedSDLGamepadsForPort(
             mPortIndex)) {
        const auto axisValue = SDL_GameControllerGetAxis(gamepad, mControllerAxis);

        auto axisMinValue = SDL_JOYSTICK_AXIS_MAX * (axisThresholdPercentage / 100.0f);
        if (((mAxisDirection == POSITIVE && axisValue > axisMinValue) ||
             (mAxisDirection == NEGATIVE && axisValue < -axisMinValue)) &&
            (!strongestDirectionOnly || LeansThisWay(gamepad, axisValue))) {
            padButtons |= mBitmask;
            leaning = true;
        }
    }
    mLeaning = leaning;
}

int8_t SDLAxisDirectionToButtonMapping::GetMappingType() {
    return MAPPING_TYPE_GAMEPAD;
}

std::string SDLAxisDirectionToButtonMapping::GetButtonMappingId() {
    return StringHelper::Sprintf("P%d-B%d-SDLA%d-AD%s", mPortIndex, mBitmask, mControllerAxis,
                                 mAxisDirection == 1 ? "P" : "N");
}

void SDLAxisDirectionToButtonMapping::SaveToConfig() {
    const std::string mappingCvarKey = CVAR_PREFIX_CONTROLLERS ".ButtonMappings." + GetButtonMappingId();
    Ship::Context::GetInstance()->GetConsoleVariables()->SetString(
        StringHelper::Sprintf("%s.ButtonMappingClass", mappingCvarKey.c_str()).c_str(),
        "SDLAxisDirectionToButtonMapping");
    Ship::Context::GetInstance()->GetConsoleVariables()->SetInteger(
        StringHelper::Sprintf("%s.Bitmask", mappingCvarKey.c_str()).c_str(), mBitmask);
    Ship::Context::GetInstance()->GetConsoleVariables()->SetInteger(
        StringHelper::Sprintf("%s.SDLControllerAxis", mappingCvarKey.c_str()).c_str(), mControllerAxis);
    Ship::Context::GetInstance()->GetConsoleVariables()->SetInteger(
        StringHelper::Sprintf("%s.AxisDirection", mappingCvarKey.c_str()).c_str(), mAxisDirection);
    Ship::Context::GetInstance()->GetConsoleVariables()->Save();
}

void SDLAxisDirectionToButtonMapping::EraseFromConfig() {
    const std::string mappingCvarKey = CVAR_PREFIX_CONTROLLERS ".ButtonMappings." + GetButtonMappingId();
    Ship::Context::GetInstance()->GetConsoleVariables()->ClearVariable(
        StringHelper::Sprintf("%s.ButtonMappingClass", mappingCvarKey.c_str()).c_str());
    Ship::Context::GetInstance()->GetConsoleVariables()->ClearVariable(
        StringHelper::Sprintf("%s.Bitmask", mappingCvarKey.c_str()).c_str());
    Ship::Context::GetInstance()->GetConsoleVariables()->ClearVariable(
        StringHelper::Sprintf("%s.SDLControllerAxis", mappingCvarKey.c_str()).c_str());
    Ship::Context::GetInstance()->GetConsoleVariables()->ClearVariable(
        StringHelper::Sprintf("%s.AxisDirection", mappingCvarKey.c_str()).c_str());
    Ship::Context::GetInstance()->GetConsoleVariables()->Save();
}

std::string SDLAxisDirectionToButtonMapping::GetPhysicalDeviceName() {
    return SDLAxisDirectionToAnyMapping::GetPhysicalDeviceName();
}

std::string SDLAxisDirectionToButtonMapping::GetPhysicalInputName() {
    return SDLAxisDirectionToAnyMapping::GetPhysicalInputName();
}
} // namespace Ship
