#pragma once

#include <cstdint>
#include <functional>
#include <string_view>

namespace Ship {

enum class OotmmFanfare {
    None,
    SfxRupee,
    SfxMinor,
    SfxMinorQuick,
    Major,
    HeartContainer,
    HeartPiece,
    Song,
    SongTime,
    SongEpona,
    SongSun,
    SongStorms,
    Mask,
    Medallion,
    Stone,
    SongZelda,
    SongSaria,
    SongLight,
    SongForest,
    SongFire,
    SongWater,
    SongShadow,
    SongSpirit,
    SongSoaring,
    SongHealing,
    SongAwakening,
    SongGoronHalf,
    SongGoron,
    SongZora,
    SongEmptiness,
    SongOrder,
};

OotmmFanfare OotmmItemFanfare_Resolve(std::string_view itemId, bool isShort,
                                      const std::function<uint32_t(std::string_view)>& owned,
                                      uint32_t strayFairyRewardCount);

}
