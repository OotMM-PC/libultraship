#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Ship {

enum class OotmmGrantGame {
    Oot,
    Mm,
    Shared,
};

enum class OotmmGrantKind {
    Equipment,
    Upgrade,
    SpinUpgrade,
    QuestFlag,
    DungeonItem,
    SmallKeyRing,
    SkeletonKey,
    Song,
    OcarinaButton,
    SilverRupee,
    Soul,
    Token,
    Triforce,
    Coin,
    Clock,
    OwlStatue,
    Map,
    RustyKey,
    Mask,
    Bottle,
    Ammo,
    Rupees,
    Health,
    Magic,
    Fishing,
    Trap,
    None,
};

struct OotmmGrantOp {
    OotmmGrantKind Kind = OotmmGrantKind::None;
    OotmmGrantGame Game = OotmmGrantGame::Shared;
    std::string Slot;
    uint32_t Amount = 0;
};

class OotmmSilverRupees {
  public:
    /// Rupees the puzzle needs in the given quest, or 0 when it has none there.
    static uint32_t Required(std::string_view suffix, bool masterQuest);
    static uint32_t MaxRequired(std::string_view suffix);
    /// Every puzzle suffix, in table order.
    static std::vector<std::string_view> All();
};

class OotmmItemGrant {
  public:
    static std::vector<OotmmGrantOp> Resolve(std::string_view itemId, uint32_t count);
    static OotmmGrantGame GameOf(std::string_view itemId);
    static std::string_view SuffixOf(std::string_view itemId);
    static bool AppliesTo(const OotmmGrantOp& op, OotmmGrantGame activeGame);
    /// True when the launcher inventory is the item's storage and consumers read it
    /// directly, so there is nothing to write into the native save.
    static bool IsInventoryBacked(OotmmGrantKind kind);
};

} // namespace Ship
