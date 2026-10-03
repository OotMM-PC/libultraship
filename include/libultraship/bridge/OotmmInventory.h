#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Ship {

enum class OotmmItemValueKind {
    Toggle,
    Counter,
    Progressive,
};

enum class OotmmItemModelKind {
    None,
    OcarinaButton,
    Soul,
    Key,
    Song,
    SongNote,
    SilverRupee,
    Coin,
    MagicalRupee,
    Triforce,
    PlatinumToken,
    Clock,
    Equipment,
    Upgrade,
    OwlStatue,
    Fishing,
    Fairy,
    Potion,
    Rupee,
    Bombchu,
    Compass,
    DungeonMap,
    Fish,
    KeyRing,
    SkeletonKey,
    MagicJar,
    Poe,
    Trap,
};

enum class OotmmOcarinaButtonModel : uint32_t {
    A,
    CUp,
    CDown,
    CLeft,
    CRight,
};

struct OotmmItemDefinition {
    std::string Id;
    std::string Name;
    std::string Category;
    OotmmItemValueKind Kind = OotmmItemValueKind::Toggle;
    uint32_t MaxValue = 1;
    OotmmItemModelKind ModelKind = OotmmItemModelKind::None;
    uint32_t ModelVariant = 0;
};

class OotmmItemCatalog {
  public:
    void Reset();
    bool Add(std::string id, std::string name);

    const std::vector<OotmmItemDefinition>& GetItems() const;
    const OotmmItemDefinition* Find(const std::string& id) const;

    static std::optional<OotmmItemDefinition> Describe(std::string id, std::string name);
    static std::optional<OotmmItemDefinition> DescribeModel(std::string id, std::string name);
    static std::string TrimDestinationSuffix(std::string name, const std::string& destinationGame);
    static std::string TrimGameSuffix(std::string name);
    static bool IsTrap(std::string_view itemId);
    static bool IsNothing(std::string_view itemId);

  private:
    std::vector<OotmmItemDefinition> mItems;
    std::unordered_map<std::string, size_t> mIndex;
};

class OotmmInventory {
  public:
    void Reset();
    void ApplySnapshot(uint64_t revision, std::unordered_map<std::string, uint32_t> values);
    bool ApplyDelta(uint64_t revision, const std::unordered_map<std::string, uint32_t>& values);

    uint64_t GetRevision() const;
    uint32_t Count(const std::string& id) const;
    bool Has(const std::string& id) const;
    const std::unordered_map<std::string, uint32_t>& GetValues() const;

  private:
    uint64_t mRevision = 0;
    std::unordered_map<std::string, uint32_t> mValues;
};

} // namespace Ship
