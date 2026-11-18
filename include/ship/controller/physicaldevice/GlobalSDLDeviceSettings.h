#pragma once

#include <cstdint>

namespace Ship {

class GlobalSDLDeviceSettings {
  public:
    GlobalSDLDeviceSettings();
    ~GlobalSDLDeviceSettings();

    int32_t GetStickAxisThresholdPercentage();
    void SetStickAxisThresholdPercentage(int32_t stickAxisThresholdPercentage);

    int32_t GetTriggerAxisThresholdPercentage();
    void SetTriggerAxisThresholdPercentage(int32_t triggerAxisThresholdPercentage);

    /// A stick with buttons on both axes presses only the button of the direction it leans toward most.
    bool GetStickButtonsStrongestDirectionOnly();
    void SetStickButtonsStrongestDirectionOnly(bool strongestDirectionOnly);

    void SaveToConfig();
    void EraseFromConfig();

  private:
    int32_t mStickAxisThresholdPercentage;
    int32_t mTriggerAxisThresholdPercentage;
    bool mStickButtonsStrongestDirectionOnly;
};
} // namespace Ship
