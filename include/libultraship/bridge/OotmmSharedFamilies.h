#pragma once

#include "libultraship/bridge/OotmmBottles.h"

#include <array>
#include <functional>
#include <string_view>
#include <utility>
#include <vector>

namespace Ship {

struct OotmmSharedFamily {
    std::string_view Setting;
    std::vector<std::string_view> Values;
    std::function<bool(std::string_view)> IsReward;
    std::string_view EnabledValue = {};
};

inline constexpr std::array<std::pair<std::string_view, std::string_view>, 7> kOotmmTraps = { {
    { "OOT_TRAP_ICE", "trapIce" },
    { "OOT_TRAP_FIRE", "trapFire" },
    { "OOT_TRAP_SHOCK", "trapShock" },
    { "OOT_TRAP_DRAIN", "trapDrain" },
    { "OOT_TRAP_ANTI_MAGIC", "trapAntiMagic" },
    { "OOT_TRAP_KNOCKBACK", "trapKnockback" },
    { "OOT_TRAP_RUPOOR", "trapRupoor" },
} };

inline std::vector<OotmmSharedFamily> BuildOotmmSharedFamilies() {
    std::vector<OotmmSharedFamily> families = {
        { "sharedBombchu", { "bombchu", "bombchuOwned", "bombchuBagFirst" },
          [](std::string_view id) { return id == "SHARED_BOMBCHU" || id.starts_with("SHARED_BOMBCHU_"); } },
        { "sharedWallets", { "rupees" },
          [](std::string_view id) {
              return id.starts_with("SHARED_RUPEE_") && !id.starts_with("SHARED_RUPEE_SILVER_") &&
                     id != "SHARED_RUPEE_MAGICAL";
          } },
        { "sharedHealth", { "health", "healthCapacity", "heartPieces" },
          [](std::string_view id) {
              return id == "SHARED_HEART_CONTAINER" || id == "SHARED_HEART_PIECE" || id == "SHARED_RECOVERY_HEART";
          } },
        { "sharedMagic", { "magic", "chateau" },
          [](std::string_view id) { return id == "SHARED_MAGIC_JAR_SMALL" || id == "SHARED_MAGIC_JAR_LARGE"; } },
        { "sharedBows", { "arrows" }, [](std::string_view id) { return id.starts_with("SHARED_ARROWS_"); } },
        { "sharedBombBags", { "bombs" }, [](std::string_view id) { return id.starts_with("SHARED_BOMBS_"); } },
        { "sharedNutsSticks", { "nuts", "sticks" },
          [](std::string_view id) { return id.starts_with("SHARED_NUTS_") || id.starts_with("SHARED_STICKS_"); } },
        { "sharedSlingshot", { "seeds" }, [](std::string_view id) { return id.starts_with("SHARED_DEKU_SEEDS_"); } },
        { "sharedPowderKeg", { "keg" }, [](std::string_view) { return false; } },
        { "sharedBottles", { kOotmmSharedBottleValues.begin(), kOotmmSharedBottleValues.end() }, OotmmIsSharedBottleReward },
        { "crossGameFw", { "fwChild", "fwAdult" }, [](std::string_view) { return false; } },
        { "erBoss", { "bossClears" }, [](std::string_view) { return false; }, "full" },
    };
    for (const auto& [id, setting] : kOotmmTraps) {
        families.push_back({ setting, { setting }, [](std::string_view) { return false; } });
    }
    return families;
}

inline std::string_view OotmmTrapForSetting(std::string_view setting) {
    for (const auto& [id, trapSetting] : kOotmmTraps) {
        if (trapSetting == setting) {
            return id;
        }
    }
    return {};
}

inline const std::vector<OotmmSharedFamily>& OotmmSharedFamilies() {
    static const std::vector<OotmmSharedFamily> families = BuildOotmmSharedFamilies();
    return families;
}

}
