#include "libultraship/bridge/OotmmItemFanfare.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

namespace Ship {

namespace {

enum class OotmmItemType {
    Major,
    Minor,
    Normal,
    Rupee,
    Heart,
    Mask,
    Skull,
    Fairy,
    Soul,
    Key,
    BossKey,
    MapCompass,
};

#include "OotmmItemTypeTable.inc"

const std::unordered_map<std::string_view, OotmmItemType>& TypeTable() {
    static const std::unordered_map<std::string_view, OotmmItemType> sTable = [] {
        std::unordered_map<std::string_view, OotmmItemType> table;
        table.reserve(std::size(kOotmmItemTypes));
        for (const auto& entry : kOotmmItemTypes) {
            table.emplace(entry.first, entry.second);
        }
        return table;
    }();
    return sTable;
}

const std::unordered_map<std::string_view, OotmmFanfare>& SpecialTable() {
    static const std::unordered_map<std::string_view, OotmmFanfare> sTable = {
        { "OCARINA", OotmmFanfare::Song },
        { "OCARINA_FAIRY", OotmmFanfare::Song },
        { "OCARINA_TIME", OotmmFanfare::Song },
        { "OCARINA_OF_TIME", OotmmFanfare::Song },
        { "STONE_EMERALD", OotmmFanfare::Stone },
        { "STONE_RUBY", OotmmFanfare::Stone },
        { "STONE_SAPPHIRE", OotmmFanfare::Stone },
        { "MEDALLION_FOREST", OotmmFanfare::Medallion },
        { "MEDALLION_FIRE", OotmmFanfare::Medallion },
        { "MEDALLION_WATER", OotmmFanfare::Medallion },
        { "MEDALLION_SPIRIT", OotmmFanfare::Medallion },
        { "MEDALLION_SHADOW", OotmmFanfare::Medallion },
        { "MEDALLION_LIGHT", OotmmFanfare::Medallion },
        { "SONG_TP_LIGHT", OotmmFanfare::SongLight },
        { "SONG_TP_FOREST", OotmmFanfare::SongForest },
        { "SONG_TP_FIRE", OotmmFanfare::SongFire },
        { "SONG_TP_WATER", OotmmFanfare::SongWater },
        { "SONG_TP_SHADOW", OotmmFanfare::SongShadow },
        { "SONG_TP_SPIRIT", OotmmFanfare::SongSpirit },
        { "SONG_ZELDA", OotmmFanfare::SongZelda },
        { "SONG_SARIA", OotmmFanfare::SongSaria },
        { "SONG_TIME", OotmmFanfare::SongTime },
        { "SONG_EPONA", OotmmFanfare::SongEpona },
        { "SONG_SUN", OotmmFanfare::SongSun },
        { "SONG_STORMS", OotmmFanfare::SongStorms },
        { "SONG_SOARING", OotmmFanfare::SongSoaring },
        { "SONG_HEALING", OotmmFanfare::SongHealing },
        { "SONG_AWAKENING", OotmmFanfare::SongAwakening },
        { "SONG_GORON_HALF", OotmmFanfare::SongGoronHalf },
        { "SONG_GORON", OotmmFanfare::SongGoron },
        { "SONG_ZORA", OotmmFanfare::SongZora },
        { "SONG_EMPTINESS", OotmmFanfare::SongEmptiness },
        { "SONG_ORDER", OotmmFanfare::SongOrder },
    };
    return sTable;
}

std::string_view Suffix(std::string_view itemId) {
    for (std::string_view prefix : { "OOT_", "MM_", "SHARED_" }) {
        if (itemId.substr(0, prefix.size()) == prefix) {
            return itemId.substr(prefix.size());
        }
    }
    return itemId;
}

OotmmItemType TypeOf(std::string_view itemId) {
    const auto& table = TypeTable();
    if (const auto entry = table.find(itemId); entry != table.end()) {
        return entry->second;
    }
    if (itemId.substr(0, 7) == "SHARED_") {
        for (std::string_view prefix : { "OOT_", "MM_" }) {
            const std::string resolved = std::string(prefix) + std::string(itemId.substr(7));
            if (const auto entry = table.find(resolved); entry != table.end()) {
                return entry->second;
            }
        }
    }
    return OotmmItemType::Major;
}

std::optional<OotmmFanfare> CountedFanfare(std::string_view itemId,
                                           const std::function<uint32_t(std::string_view)>& owned,
                                           uint32_t strayFairyRewardCount) {
    if (itemId == "OOT_HEART_PIECE" || itemId == "OOT_TC_HEART_PIECE") {
        if ((owned("OOT_HEART_PIECE") + owned("OOT_TC_HEART_PIECE")) % 4 != 0) {
            return OotmmFanfare::HeartPiece;
        }
        return std::nullopt;
    }
    if (itemId == "MM_HEART_PIECE") {
        if (owned("MM_HEART_PIECE") % 4 != 0) {
            return OotmmFanfare::HeartPiece;
        }
        return std::nullopt;
    }
    if (itemId == "MM_GS_TOKEN_SWAMP" || itemId == "MM_GS_TOKEN_OCEAN") {
        if (owned(itemId) >= 30) {
            return OotmmFanfare::Major;
        }
        return std::nullopt;
    }
    if (itemId == "OOT_GS_TOKEN") {
        const uint32_t tokens = owned(itemId);
        if (tokens == 10 || tokens == 20 || tokens == 30 || tokens == 40 || tokens == 50 || tokens >= 100) {
            return OotmmFanfare::Major;
        }
        return std::nullopt;
    }
    if (itemId == "MM_STRAY_FAIRY_TOWN") {
        return OotmmFanfare::Major;
    }
    if (itemId.substr(0, 15) == "MM_STRAY_FAIRY_" && itemId.size() > 15) {
        if (owned(itemId) == strayFairyRewardCount) {
            return OotmmFanfare::Major;
        }
        return std::nullopt;
    }
    return std::nullopt;
}

}

OotmmFanfare OotmmItemFanfare_Resolve(std::string_view itemId, bool isShort,
                                      const std::function<uint32_t(std::string_view)>& owned,
                                      uint32_t strayFairyRewardCount) {
    if (itemId.empty() || itemId == "NOTHING") {
        return OotmmFanfare::None;
    }
    if (const auto special = SpecialTable().find(Suffix(itemId)); special != SpecialTable().end()) {
        return special->second;
    }
    if (const auto counted = CountedFanfare(itemId, owned, strayFairyRewardCount)) {
        return *counted;
    }
    switch (TypeOf(itemId)) {
        case OotmmItemType::Mask:
            return OotmmFanfare::Mask;
        case OotmmItemType::Minor:
            return isShort ? OotmmFanfare::SfxMinorQuick : OotmmFanfare::SfxMinor;
        case OotmmItemType::Rupee:
            return isShort ? OotmmFanfare::SfxRupee : OotmmFanfare::SfxMinor;
        case OotmmItemType::Heart:
            return OotmmFanfare::HeartContainer;
        case OotmmItemType::Skull:
        case OotmmItemType::Fairy:
            return OotmmFanfare::HeartPiece;
        default:
            return OotmmFanfare::Major;
    }
}

}
