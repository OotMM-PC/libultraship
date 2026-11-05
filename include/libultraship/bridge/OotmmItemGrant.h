#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Ship {

class OotmmInventory;

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
    BigFairy,
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
    static uint32_t Required(std::string_view suffix, bool masterQuest);
    static uint32_t MaxRequired(std::string_view suffix);
    static std::vector<std::string_view> All();
};

class OotmmItemGrant {
  public:
    static std::vector<OotmmGrantOp> Resolve(std::string_view itemId, uint32_t count);
    static std::string RenewalOf(std::string_view itemId);
    static bool CanHoldRemote(std::string_view itemId, const OotmmInventory& inventory, const std::string& vitalsPath);
    static const std::vector<std::string>& VitalItems(OotmmGrantGame game);
    static std::string VitalFamily(std::string_view itemId);
    static int32_t VitalUnit(std::string_view itemId, bool sharedMagic);
    static std::string BuildVitals(OotmmGrantGame game, const std::function<int32_t(std::string_view)>& freeOf);
    static std::optional<int32_t> RemoteRoom(std::string_view family, const std::string& vitalsPath);
    static void NoteRemoteGrant(std::string_view itemId, const std::string& vitalsPath, bool sharedMagic);
    static void WriteVitals(const std::string& path, const std::string& body);
    static OotmmGrantGame GameOf(std::string_view itemId);
    static std::string_view SuffixOf(std::string_view itemId);
    static bool AppliesTo(const OotmmGrantOp& op, OotmmGrantGame activeGame);
    static bool IsInventoryBacked(OotmmGrantKind kind);
};

} // namespace Ship
