#include "libultraship/bridge/OotmmInventory.h"

#include "libultraship/bridge/OotmmItemGrant.h"

#include <algorithm>
#include <array>
#include <string_view>

namespace Ship {

namespace {

bool StartsWith(std::string_view value, std::string_view prefix) {
    return value.starts_with(prefix);
}

bool Contains(std::string_view value, std::string_view part) {
    return value.find(part) != std::string_view::npos;
}

bool EndsWith(std::string_view value, std::string_view suffix) {
    return value.ends_with(suffix);
}

uint32_t SongNoteMaximum(std::string_view id) {
    if (Contains(id, "_GORON")) {
        return 8;
    }
    if (Contains(id, "_AWAKENING") || Contains(id, "_EMPTINESS") || Contains(id, "_ZORA") ||
        Contains(id, "_TP_SHADOW")) {
        return 7;
    }
    if (Contains(id, "_TP_FIRE")) {
        return 8;
    }
    if (Contains(id, "_TP_WATER")) {
        return 5;
    }
    return 6;
}

bool IsCustomSong(std::string_view id) {
    if (id.starts_with("SHARED_SONG_")) {
        const std::string_view tail = id.substr(7);
        std::string oot("OOT_");
        oot += tail;
        std::string mm("MM_");
        mm += tail;
        return IsCustomSong(oot) || IsCustomSong(mm);
    }
    static constexpr std::array<std::string_view, 7> kOotSongs = {
        "OOT_SONG_EMPTINESS", "OOT_SONG_HEALING", "OOT_SONG_SOARING", "OOT_SONG_AWAKENING",
        "OOT_SONG_GORON", "OOT_SONG_ZORA", "OOT_SONG_ORDER",
    };
    if (std::find(kOotSongs.begin(), kOotSongs.end(), id) != kOotSongs.end()) {
        return true;
    }
    return id == "MM_SONG_ZELDA" || id == "MM_SONG_SARIA" || id == "MM_SONG_SUN" ||
           StartsWith(id, "MM_SONG_TP_");
}

bool IsCrossGameAbility(std::string_view id) {
    static constexpr std::array<std::string_view, 47> kItems = {
        "MM_MASK_GERUDO",
        "MM_MASK_SKULL",
        "MM_MASK_SPOOKY",
        "MM_BOTTLE_RUTO_LETTER",
        "SHARED_MASK_GERUDO",
        "SHARED_MASK_SKULL",
        "SHARED_MASK_SPOOKY",
        "SHARED_BOTTLE_RUTO_LETTER",
        "MM_HAMMER",
        "MM_SLINGSHOT",
        "MM_BOOMERANG",
        "MM_BOOTS_IRON",
        "MM_BOOTS_HOVER",
        "MM_TUNIC_GORON",
        "MM_TUNIC_ZORA",
        "MM_STONE_OF_AGONY",
        "MM_SHIELD_DEKU",
        "MM_SPELL_FIRE",
        "MM_SPELL_WIND",
        "MM_SPELL_LOVE",
        "OOT_GREAT_FAIRY_SWORD",
        "OOT_POWDER_KEG",
        "OOT_MASK_BLAST",
        "OOT_MASK_STONE",
        "OOT_MASK_KAMARO",
        "OOT_SWORD_RAZOR",
        "OOT_SWORD_GILDED",
        "OOT_BOMBCHU_BAG",
        "SHARED_HAMMER",
        "SHARED_SLINGSHOT",
        "SHARED_BOOMERANG",
        "SHARED_BOOTS_IRON",
        "SHARED_BOOTS_HOVER",
        "SHARED_TUNIC_GORON",
        "SHARED_TUNIC_ZORA",
        "SHARED_STONE_OF_AGONY",
        "SHARED_SPELL_FIRE",
        "SHARED_SPELL_WIND",
        "SHARED_SPELL_LOVE",
        "SHARED_GREAT_FAIRY_SWORD",
        "SHARED_POWDER_KEG",
        "SHARED_MASK_BLAST",
        "SHARED_MASK_STONE",
        "SHARED_MASK_KAMARO",
        "SHARED_BOMBCHU_BAG",
        "SHARED_SHIELD_DEKU",
        "SHARED_SHIELD_HYLIAN",
    };
    return std::find(kItems.begin(), kItems.end(), id) != kItems.end();
}

std::string DisplayName(std::string id, std::string name) {
    return name.empty() ? std::move(id) : std::move(name);
}

constexpr std::array<std::string_view, 38> kFastOot = {
    "RECOVERY_HEART", "STICK", "STICKS_5", "STICKS_10", "NUTS_5", "NUTS_5_ALT", "NUTS_10", "BOMB", "BOMBS_5",
    "BOMBS_10", "BOMBS_20", "BOMBS_30", "BOMBCHU_5", "BOMBCHU_10", "BOMBCHU_20", "ARROWS_5", "ARROWS_10",
    "ARROWS_30", "DEKU_SEEDS_5", "DEKU_SEEDS_30", "RUPEE_GREEN", "RUPEE_BLUE", "RUPEE_RED", "RUPEE_PURPLE",
    "RUPEE_HUGE", "POTION_RED", "POTION_GREEN", "POTION_BLUE", "FISH", "BUGS", "BLUE_FIRE", "POE", "FAIRY", "MILK",
    "MAGIC_JAR_SMALL", "MAGIC_JAR_LARGE", "BIG_POE", "FAIRY_BIG"
};

constexpr std::array<std::string_view, 36> kFastMm = {
    "RECOVERY_HEART", "STICK", "NUT", "NUTS_5", "NUTS_10", "BOMB", "BOMBS_5", "BOMBS_10", "BOMBS_20", "BOMBS_30",
    "BOMBCHU", "BOMBCHU_5", "BOMBCHU_10", "BOMBCHU_20", "ARROWS_10", "ARROWS_30", "ARROWS_40", "RUPEE_GREEN",
    "RUPEE_BLUE", "RUPEE_RED", "RUPEE_PURPLE", "RUPEE_SILVER", "RUPEE_GOLD", "POTION_RED", "POTION_GREEN",
    "POTION_BLUE", "FAIRY", "MILK", "CHATEAU", "FISH", "BUGS", "POE", "MAGIC_JAR_SMALL", "MAGIC_JAR_LARGE",
    "BIG_POE", "FAIRY_BIG"
};

bool InFastList(std::string_view base, std::string_view game) {
    const auto matches = [base](const auto& list) { return std::find(list.begin(), list.end(), base) != list.end(); };
    if (game == "OOT") {
        return matches(kFastOot);
    }
    if (game == "MM") {
        return matches(kFastMm);
    }
    return matches(kFastOot) || matches(kFastMm);
}

} // namespace

void OotmmItemCatalog::Reset() {
    mItems.clear();
    mIndex.clear();
}

bool OotmmItemCatalog::Add(std::string id, std::string name) {
    if (mIndex.contains(id)) {
        return false;
    }
    auto item = Describe(std::move(id), std::move(name));
    if (!item.has_value()) {
        return false;
    }
    mIndex.emplace(item->Id, mItems.size());
    mItems.emplace_back(std::move(*item));
    return true;
}

const std::vector<OotmmItemDefinition>& OotmmItemCatalog::GetItems() const {
    return mItems;
}

const OotmmItemDefinition* OotmmItemCatalog::Find(const std::string& id) const {
    const auto item = mIndex.find(id);
    return item == mIndex.end() ? nullptr : &mItems[item->second];
}

std::optional<OotmmItemDefinition> OotmmItemCatalog::Describe(std::string id, std::string name) {
    const std::string_view value = id;
    OotmmItemDefinition item{
        .Id = id,
        .Name = DisplayName(id, std::move(name)),
    };

    if (EndsWith(value, "_BOMBCHU_BAG")) {
        item.Category = "Upgrades";
        item.Kind = OotmmItemValueKind::Progressive;
        item.MaxValue = 3;
        item.ModelKind = OotmmItemModelKind::Bombchu;
    } else if (Contains(value, "_SOUL_")) {
        item.Category = "Souls";
        item.ModelKind = OotmmItemModelKind::Soul;
    } else if (Contains(value, "_BUTTON_")) {
        item.Category = "Ocarina Buttons";
        item.ModelKind = OotmmItemModelKind::OcarinaButton;
        if (EndsWith(value, "_C_UP")) {
            item.ModelVariant = static_cast<uint32_t>(OotmmOcarinaButtonModel::CUp);
        } else if (EndsWith(value, "_C_DOWN")) {
            item.ModelVariant = static_cast<uint32_t>(OotmmOcarinaButtonModel::CDown);
        } else if (EndsWith(value, "_C_LEFT")) {
            item.ModelVariant = static_cast<uint32_t>(OotmmOcarinaButtonModel::CLeft);
        } else if (EndsWith(value, "_C_RIGHT")) {
            item.ModelVariant = static_cast<uint32_t>(OotmmOcarinaButtonModel::CRight);
        } else {
            item.ModelVariant = static_cast<uint32_t>(OotmmOcarinaButtonModel::A);
        }
    } else if (Contains(value, "_SONG_NOTE_")) {
        item.Category = "Song Notes";
        item.Kind = OotmmItemValueKind::Counter;
        item.MaxValue = SongNoteMaximum(value);
        item.ModelKind = OotmmItemModelKind::SongNote;
    } else if (Contains(value, "_POUCH_SILVER_")) {
        item.Category = "Silver Rupees";
        item.Kind = OotmmItemValueKind::Toggle;
        item.MaxValue = 1;
        item.ModelKind = OotmmItemModelKind::SilverRupee;
    } else if (Contains(value, "_RUPEE_SILVER_")) {
        constexpr std::string_view marker = "_RUPEE_SILVER_";
        const std::string_view puzzle =
            std::string_view(value).substr(value.find(marker) + marker.size());
        item.Category = "Silver Rupees";
        item.Kind = OotmmItemValueKind::Counter;
        item.MaxValue = std::max(OotmmSilverRupees::MaxRequired(puzzle), 1u);
        item.ModelKind = OotmmItemModelKind::SilverRupee;
    } else if (Contains(value, "_RUSTY_KEY_")) {
        item.Category = "Rusty Keys";
        item.ModelKind = OotmmItemModelKind::Key;
    } else if (Contains(value, "_KEY_RING_") || EndsWith(value, "_SKELETON_KEY")) {
        item.Category = "Keys";
        item.ModelKind = OotmmItemModelKind::Key;
    } else if (StartsWith(value, "MM_CLOCK")) {
        item.Category = "Clocks";
        item.Kind = value == "MM_CLOCK" ? OotmmItemValueKind::Progressive : OotmmItemValueKind::Toggle;
        item.MaxValue = value == "MM_CLOCK" ? 6 : 1;
        item.ModelKind = OotmmItemModelKind::Clock;
    } else if (StartsWith(value, "OOT_COIN_")) {
        item.Category = "Coins";
        item.Kind = OotmmItemValueKind::Counter;
        item.MaxValue = 999;
        item.ModelKind = OotmmItemModelKind::Coin;
        if (EndsWith(value, "_RED")) {
            item.ModelVariant = 1;
        } else if (EndsWith(value, "_GREEN")) {
            item.ModelVariant = 2;
        } else if (EndsWith(value, "_BLUE")) {
            item.ModelVariant = 3;
        }
    } else if (EndsWith(value, "_RUPEE_MAGICAL")) {
        item.Category = "Silver Rupees";
        item.Kind = OotmmItemValueKind::Toggle;
        item.MaxValue = 1;
        item.ModelKind = OotmmItemModelKind::MagicalRupee;
    } else if (Contains(value, "TRIFORCE")) {
        item.Category = "Triforce";
        item.Kind = OotmmItemValueKind::Counter;
        item.MaxValue = 999;
        item.ModelKind = OotmmItemModelKind::Triforce;
    } else if (EndsWith(value, "_PLATINUM_TOKEN")) {
        item.Category = "Tokens";
        item.Kind = OotmmItemValueKind::Counter;
        item.MaxValue = 999;
        item.ModelKind = OotmmItemModelKind::PlatinumToken;
    } else if (IsCustomSong(value)) {
        item.Category = "Songs";
        item.ModelKind = OotmmItemModelKind::Song;
    } else if (IsCrossGameAbility(value)) {
        item.Category = "Cross-Game Equipment";
        item.ModelKind = OotmmItemModelKind::Equipment;
    } else if (value == "MM_SCALE" || value == "MM_STRENGTH" || value == "SHARED_SCALE" ||
               value == "SHARED_STRENGTH" || EndsWith(value, "_SPIN_UPGRADE")) {
        item.Category = "Upgrades";
        item.ModelKind = OotmmItemModelKind::Upgrade;
        if (!EndsWith(value, "_SPIN_UPGRADE")) {
            item.Kind = OotmmItemValueKind::Progressive;
            item.MaxValue = 3;
        }
    } else if (StartsWith(value, "MM_OWL_")) {
        item.Category = "Owl Statues";
        item.ModelKind = OotmmItemModelKind::OwlStatue;
    } else if (StartsWith(value, "OOT_FISHING_POND_")) {
        item.Category = "Fishing";
        item.Kind = OotmmItemValueKind::Counter;
        item.MaxValue = 99;
        item.ModelKind = OotmmItemModelKind::Fishing;
    } else if (value == "MM_TRANSCENDENT_FAIRY") {
        item.Category = "Other";
        item.ModelKind = OotmmItemModelKind::Fairy;
    } else {
        return std::nullopt;
    }

    return item;
}

std::optional<OotmmItemDefinition> OotmmItemCatalog::DescribeModel(std::string id, std::string name) {
    const std::string_view value = id;
    if (EndsWith(value, "_SKELETON_KEY") || Contains(value, "_KEY_RING")) {
        OotmmItemDefinition item{
            .Id = id,
            .Name = DisplayName(id, std::move(name)),
            .ModelKind = EndsWith(value, "_SKELETON_KEY") ? OotmmItemModelKind::SkeletonKey
                                                          : OotmmItemModelKind::KeyRing,
        };
        return item;
    }
    if (auto custom = Describe(id, name)) {
        return custom;
    }

    OotmmItemDefinition item{
        .Id = id,
        .Name = DisplayName(id, std::move(name)),
    };

    if (Contains(value, "_POTION_")) {
        item.ModelKind = OotmmItemModelKind::Potion;
        item.ModelVariant = Contains(value, "_GREEN") ? 1 : Contains(value, "_BLUE") ? 2 : 0;
    } else if (Contains(value, "_RUPEE_")) {
        item.ModelKind = OotmmItemModelKind::Rupee;
        item.ModelVariant = Contains(value, "_BLUE")   ? 1
                            : Contains(value, "_RED")  ? 2
                            : Contains(value, "_PURPLE") ? 3
                            : Contains(value, "_SILVER") ? 4
                            : Contains(value, "_GOLD") || Contains(value, "_HUGE") ? 5
                                                                                   : 0;
    } else if (Contains(value, "_BOMBCHU")) {
        item.ModelKind = OotmmItemModelKind::Bombchu;
    } else if (Contains(value, "_COMPASS_")) {
        item.ModelKind = OotmmItemModelKind::Compass;
    } else if (Contains(value, "_WORLD_MAP_") || Contains(value, "_MAP_")) {
        item.ModelKind = OotmmItemModelKind::DungeonMap;
    } else if (EndsWith(value, "_FISH")) {
        item.ModelKind = OotmmItemModelKind::Fish;
    } else if (Contains(value, "_MAGIC_JAR_")) {
        item.ModelKind = OotmmItemModelKind::MagicJar;
        item.ModelVariant = Contains(value, "_LARGE") ? 1 : 0;
    } else if (EndsWith(value, "_BOTTLE_POE") || EndsWith(value, "_POE")) {
        item.ModelKind = OotmmItemModelKind::Poe;
    } else if (Contains(value, "_STRAY_FAIRY_")) {
        item.ModelKind = OotmmItemModelKind::Fairy;
    } else if (Contains(value, "_TRAP_")) {
        item.ModelKind = OotmmItemModelKind::Trap;
        item.ModelVariant = Contains(value, "_FIRE")         ? 1
                            : Contains(value, "_SHOCK")      ? 2
                            : Contains(value, "_DRAIN")      ? 3
                            : Contains(value, "_ANTI_MAGIC") ? 4
                            : Contains(value, "_KNOCKBACK")  ? 5
                            : Contains(value, "_RUPOOR")     ? 6
                                                             : 0;
    } else {
        return std::nullopt;
    }

    return item;
}

std::string OotmmItemCatalog::TrimDestinationSuffix(std::string name, const std::string& destinationGame) {
    const std::string suffix = " (" + destinationGame + ")";
    if (name.ends_with(suffix)) {
        name.resize(name.size() - suffix.size());
    }
    return name;
}

std::string OotmmItemCatalog::TrimGameSuffix(std::string name) {
    for (const std::string& game : { "MM", "OoT", "OOT" }) {
        name = TrimDestinationSuffix(std::move(name), game);
    }
    return name;
}

bool OotmmItemCatalog::IsTrap(std::string_view itemId) {
    return itemId.find("_TRAP_") != std::string_view::npos;
}

bool OotmmItemCatalog::IsNothing(std::string_view itemId) {
    return itemId == "NOTHING";
}

bool OotmmItemCatalog::IsFast(std::string_view itemId) {
    for (const std::string_view game : { "OOT", "MM", "SHARED" }) {
        if (itemId.size() > game.size() + 1 && StartsWith(itemId, game) && itemId[game.size()] == '_') {
            return InFastList(itemId.substr(game.size() + 1), game);
        }
    }
    return false;
}

void OotmmInventory::Reset() {
    mRevision = 0;
    mValues.clear();
}

void OotmmInventory::ApplySnapshot(uint64_t revision, std::unordered_map<std::string, uint32_t> values) {
    if (revision < mRevision) {
        return;
    }
    mRevision = revision;
    mValues = std::move(values);
}

bool OotmmInventory::ApplyDelta(uint64_t revision, const std::unordered_map<std::string, uint32_t>& values) {
    if (revision <= mRevision) {
        return false;
    }
    for (const auto& [id, value] : values) {
        if (value == 0) {
            mValues.erase(id);
        } else {
            mValues[id] = value;
        }
    }
    mRevision = revision;
    return true;
}

uint64_t OotmmInventory::GetRevision() const {
    return mRevision;
}

uint32_t OotmmInventory::Count(const std::string& id) const {
    const auto value = mValues.find(id);
    return value == mValues.end() ? 0 : value->second;
}

bool OotmmInventory::Has(const std::string& id) const {
    return Count(id) != 0;
}

const std::unordered_map<std::string, uint32_t>& OotmmInventory::GetValues() const {
    return mValues;
}

} // namespace Ship
