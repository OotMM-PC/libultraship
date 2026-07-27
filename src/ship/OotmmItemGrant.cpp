#include "libultraship/bridge/OotmmItemGrant.h"

#include <algorithm>

namespace Ship {

namespace {

bool Contains(std::string_view value, std::string_view needle) {
    return value.find(needle) != std::string_view::npos;
}

OotmmGrantOp Op(OotmmGrantKind kind, OotmmGrantGame game, std::string_view slot, uint32_t amount) {
    return OotmmGrantOp{ kind, game, std::string(slot), amount };
}

} // namespace

OotmmGrantGame OotmmItemGrant::GameOf(std::string_view itemId) {
    if (itemId.starts_with("OOT_")) {
        return OotmmGrantGame::Oot;
    }
    if (itemId.starts_with("MM_")) {
        return OotmmGrantGame::Mm;
    }
    return OotmmGrantGame::Shared;
}

std::string_view OotmmItemGrant::SuffixOf(std::string_view itemId) {
    for (const std::string_view prefix : { "OOT_", "MM_", "SHARED_" }) {
        if (itemId.starts_with(prefix)) {
            return itemId.substr(prefix.size());
        }
    }
    return itemId;
}

bool OotmmItemGrant::AppliesTo(const OotmmGrantOp& op, OotmmGrantGame activeGame) {
    return op.Game == OotmmGrantGame::Shared || op.Game == activeGame;
}

bool OotmmItemGrant::IsInventoryBacked(OotmmGrantKind kind) {
    switch (kind) {
        case OotmmGrantKind::Soul:
        case OotmmGrantKind::RustyKey:
        case OotmmGrantKind::OcarinaButton:
        case OotmmGrantKind::SilverRupee:
        case OotmmGrantKind::Triforce:
        case OotmmGrantKind::Coin:
        case OotmmGrantKind::Fishing:
        case OotmmGrantKind::Trap:
            return true;
        default:
            return false;
    }
}

std::vector<OotmmGrantOp> OotmmItemGrant::Resolve(std::string_view itemId, uint32_t count) {
    std::vector<OotmmGrantOp> ops;
    if (count == 0) {
        return ops;
    }

    const OotmmGrantGame game = GameOf(itemId);
    const std::string_view suffix = SuffixOf(itemId);

    if (Contains(itemId, "_SOUL_")) {
        ops.push_back(Op(OotmmGrantKind::Soul, game, suffix, 1));
        return ops;
    }
    if (Contains(itemId, "_RUSTY_KEY_")) {
        ops.push_back(Op(OotmmGrantKind::RustyKey, game, suffix, 1));
        return ops;
    }
    if (Contains(itemId, "_BUTTON_") || Contains(itemId, "_SONG_NOTE_")) {
        ops.push_back(Op(OotmmGrantKind::OcarinaButton, game, suffix, count));
        return ops;
    }
    if (Contains(itemId, "_RUPEE_SILVER_") || Contains(itemId, "_POUCH_SILVER_")) {
        ops.push_back(Op(OotmmGrantKind::SilverRupee, game, suffix, count));
        return ops;
    }
    if (Contains(itemId, "_KEY_RING_")) {
        ops.push_back(Op(OotmmGrantKind::SmallKeyRing, game, suffix, 1));
        return ops;
    }
    if (suffix == "SKELETON_KEY") {
        ops.push_back(Op(OotmmGrantKind::SkeletonKey, game, suffix, 1));
        return ops;
    }
    if (Contains(itemId, "_WORLD_MAP_")) {
        ops.push_back(Op(OotmmGrantKind::Map, game, suffix, 1));
        return ops;
    }
    if (Contains(itemId, "_MAP_") || Contains(itemId, "_COMPASS_") ||
        Contains(itemId, "_BOSS_KEY_") || Contains(itemId, "_SMALL_KEY_") ||
        Contains(itemId, "_STRAY_FAIRY_")) {
        ops.push_back(Op(OotmmGrantKind::DungeonItem, game, suffix, count));
        return ops;
    }
    if (Contains(itemId, "_OWL_")) {
        ops.push_back(Op(OotmmGrantKind::OwlStatue, game, suffix, 1));
        return ops;
    }
    if (suffix.starts_with("CLOCK")) {
        ops.push_back(Op(OotmmGrantKind::Clock, game, suffix, count));
        return ops;
    }
    if (Contains(itemId, "TRIFORCE")) {
        ops.push_back(Op(OotmmGrantKind::Triforce, game, suffix, count));
        return ops;
    }
    if (Contains(itemId, "_COIN_")) {
        ops.push_back(Op(OotmmGrantKind::Coin, game, suffix, count));
        return ops;
    }
    if (Contains(itemId, "GS_TOKEN") || Contains(itemId, "PLATINUM_TOKEN")) {
        ops.push_back(Op(OotmmGrantKind::Token, game, suffix, count));
        return ops;
    }
    if (Contains(itemId, "_SONG_")) {
        ops.push_back(Op(OotmmGrantKind::Song, game, suffix, 1));
        return ops;
    }
    if (Contains(itemId, "_TRAP_")) {
        ops.push_back(Op(OotmmGrantKind::Trap, game, suffix, count));
        return ops;
    }
    if (Contains(itemId, "_MASK_")) {
        ops.push_back(Op(OotmmGrantKind::Mask, game, suffix, 1));
        return ops;
    }
    if (Contains(itemId, "_BOTTLE") || Contains(itemId, "_POTION_") ||
        Contains(itemId, "_CHATEAU") || Contains(itemId, "_MILK")) {
        ops.push_back(Op(OotmmGrantKind::Bottle, game, suffix, count));
        return ops;
    }
    if (Contains(itemId, "FISHING_POND")) {
        ops.push_back(Op(OotmmGrantKind::Fishing, game, suffix, count));
        return ops;
    }
    if (Contains(itemId, "_RUPEE")) {
        ops.push_back(Op(OotmmGrantKind::Rupees, game, suffix, count));
        return ops;
    }
    if (Contains(itemId, "_MAGIC_JAR_") || Contains(itemId, "_MAGIC_UPGRADE")) {
        ops.push_back(Op(OotmmGrantKind::Magic, game, suffix, count));
        return ops;
    }
    if (Contains(itemId, "_RECOVERY_HEART") || Contains(itemId, "_HEART_")) {
        ops.push_back(Op(OotmmGrantKind::Health, game, suffix, count));
        return ops;
    }
    if (Contains(itemId, "_ARROWS_") || Contains(itemId, "_BOMBS_") ||
        Contains(itemId, "_BOMBCHU_") || Contains(itemId, "_NUTS_") ||
        Contains(itemId, "_STICKS_") || Contains(itemId, "_SEEDS_")) {
        ops.push_back(Op(OotmmGrantKind::Ammo, game, suffix, count));
        return ops;
    }
    if (Contains(itemId, "_MEDALLION_") || Contains(itemId, "_STONE_") ||
        Contains(itemId, "_REMAINS_")) {
        ops.push_back(Op(OotmmGrantKind::QuestFlag, game, suffix, 1));
        return ops;
    }
    if (Contains(itemId, "_UPGRADE") || Contains(itemId, "_WALLET") ||
        Contains(itemId, "_SCALE") || Contains(itemId, "_STRENGTH")) {
        ops.push_back(Op(OotmmGrantKind::Upgrade, game, suffix, count));
        return ops;
    }

    ops.push_back(Op(OotmmGrantKind::Equipment, game, suffix, count));
    return ops;
}

} // namespace Ship
