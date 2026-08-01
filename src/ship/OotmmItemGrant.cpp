#include "libultraship/bridge/OotmmItemGrant.h"

#include <algorithm>
#include <iterator>

namespace Ship {

namespace {

bool Contains(std::string_view value, std::string_view needle) {
    return value.find(needle) != std::string_view::npos;
}

OotmmGrantOp Op(OotmmGrantKind kind, OotmmGrantGame game, std::string_view slot, uint32_t amount) {
    return OotmmGrantOp{ kind, game, std::string(slot), amount };
}

struct SilverRupeePuzzle {
    std::string_view Suffix;
    uint32_t Vanilla;
    uint32_t MasterQuest;
};

constexpr SilverRupeePuzzle kSilverRupeePuzzles[] = {
    { "DC", 0, 5 },
    { "BOTW", 5, 0 },
    { "SPIRIT_CHILD", 5, 0 },
    { "SPIRIT_SUN", 5, 0 },
    { "SPIRIT_BOULDERS", 5, 0 },
    { "SPIRIT_LOBBY", 0, 5 },
    { "SPIRIT_ADULT", 0, 5 },
    { "SHADOW_SCYTHE", 5, 5 },
    { "SHADOW_PIT", 5, 5 },
    { "SHADOW_SPIKES", 5, 10 },
    { "SHADOW_BLADES", 0, 10 },
    { "IC_SCYTHE", 5, 0 },
    { "IC_BLOCK", 5, 0 },
    { "GTG_SLOPES", 5, 5 },
    { "GTG_LAVA", 5, 6 },
    { "GTG_WATER", 5, 3 },
    { "GANON_SPIRIT", 5, 0 },
    { "GANON_LIGHT", 5, 0 },
    { "GANON_FIRE", 5, 5 },
    { "GANON_FOREST", 5, 0 },
    { "GANON_SHADOW", 0, 5 },
    { "GANON_WATER", 0, 5 },
};

const SilverRupeePuzzle* FindSilverRupeePuzzle(std::string_view suffix) {
    for (const SilverRupeePuzzle& puzzle : kSilverRupeePuzzles) {
        if (puzzle.Suffix == suffix) {
            return &puzzle;
        }
    }
    return nullptr;
}

} // namespace

uint32_t OotmmSilverRupees::Required(std::string_view suffix, bool masterQuest) {
    const SilverRupeePuzzle* puzzle = FindSilverRupeePuzzle(suffix);
    if (puzzle == nullptr) {
        return 0;
    }
    return masterQuest ? puzzle->MasterQuest : puzzle->Vanilla;
}

uint32_t OotmmSilverRupees::MaxRequired(std::string_view suffix) {
    const SilverRupeePuzzle* puzzle = FindSilverRupeePuzzle(suffix);
    return puzzle == nullptr ? 0 : std::max(puzzle->Vanilla, puzzle->MasterQuest);
}

std::vector<std::string_view> OotmmSilverRupees::All() {
    std::vector<std::string_view> suffixes;
    suffixes.reserve(std::size(kSilverRupeePuzzles));
    for (const SilverRupeePuzzle& puzzle : kSilverRupeePuzzles) {
        suffixes.push_back(puzzle.Suffix);
    }
    return suffixes;
}

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
        case OotmmGrantKind::SkeletonKey:
        case OotmmGrantKind::SpinUpgrade:
        case OotmmGrantKind::OcarinaButton:
        case OotmmGrantKind::SilverRupee:
        case OotmmGrantKind::Triforce:
        case OotmmGrantKind::Coin:
        case OotmmGrantKind::Clock:
        case OotmmGrantKind::OwlStatue:
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
    // Must precede the generic _RUPEE branch, which would spend it as wallet money.
    if (Contains(itemId, "_RUPEE_SILVER_") || Contains(itemId, "_POUCH_SILVER_") ||
        suffix == "RUPEE_MAGICAL") {
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
        ops.push_back(Op(OotmmGrantKind::Map, OotmmGrantGame::Mm, suffix, 1));
        return ops;
    }
    if (Contains(itemId, "_MAP_") || Contains(itemId, "_COMPASS_") ||
        Contains(itemId, "_BOSS_KEY_") || Contains(itemId, "_SMALL_KEY_") ||
        Contains(itemId, "_STRAY_FAIRY_")) {
        ops.push_back(Op(OotmmGrantKind::DungeonItem, game, suffix, count));
        return ops;
    }
    if (Contains(itemId, "_OWL_")) {
        ops.push_back(Op(OotmmGrantKind::OwlStatue, OotmmGrantGame::Mm, suffix, 1));
        return ops;
    }
    if (suffix.starts_with("CLOCK")) {
        ops.push_back(Op(OotmmGrantKind::Clock, OotmmGrantGame::Mm, suffix, count));
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
    if (suffix == "SPIN_UPGRADE") {
        ops.push_back(Op(OotmmGrantKind::SpinUpgrade, game, suffix, 1));
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
