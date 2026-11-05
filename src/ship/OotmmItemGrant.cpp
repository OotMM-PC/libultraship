#include "libultraship/bridge/OotmmItemGrant.h"
#include "libultraship/bridge/OotmmInventory.h"
#include "libultraship/bridge/OotmmMagic.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
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

std::string OotmmItemGrant::RenewalOf(std::string_view itemId) {
    static constexpr std::string_view kShared[] = {
        "RECOVERY_HEART", "STICK", "NUTS_5", "NUTS_10", "BOMB", "BOMBS_5", "BOMBS_10", "BOMBS_20", "BOMBS_30",
        "ARROWS_10", "ARROWS_30", "DEKU_SEEDS_5", "DEKU_SEEDS_30", "BOMBCHU_5", "BOMBCHU_10", "BOMBCHU_20",
        "MILK", "POTION_RED", "POTION_GREEN", "POTION_BLUE", "FAIRY", "FISH", "BUGS", "POE",
        "MAGIC_JAR_SMALL", "MAGIC_JAR_LARGE", "FAIRY_BIG",
    };
    static constexpr std::string_view kOot[] = {
        "STICKS_5", "STICKS_10", "NUTS_5_ALT", "ARROWS_5", "SHIELD_DEKU", "SHIELD_HYLIAN", "BLUE_FIRE",
    };
    static constexpr std::string_view kMm[] = {
        "NUT", "BOMBCHU", "ARROWS_40", "SHIELD_HERO", "CHATEAU", "MAGIC_BEAN",
    };
    const bool oot = itemId.starts_with("OOT_");
    if (!oot && !itemId.starts_with("MM_")) {
        return {};
    }
    const std::string_view prefix = oot ? "OOT_" : "MM_";
    std::string_view base = itemId.substr(prefix.size());
    if (base.starts_with("BOTTLE_")) {
        base.remove_prefix(7);
        if (base == "EMPTY" || base == "BIG_POE") {
            return base == "BIG_POE" ? std::string(prefix) + "BIG_POE" : std::string();
        }
    }
    const auto listed = [base](const auto& list) {
        return std::find(std::begin(list), std::end(list), base) != std::end(list);
    };
    if (listed(kShared) || (oot ? listed(kOot) : listed(kMm))) {
        return std::string(prefix) + std::string(base);
    }
    return {};
}

const std::vector<std::string>& OotmmItemGrant::VitalItems(OotmmGrantGame game) {
    static const auto build = [](const char* prefix, std::initializer_list<const char*> names) {
        std::vector<std::string> ids;
        for (const char* name : names) {
            ids.push_back(std::string(prefix) + name);
        }
        return ids;
    };
    static const std::vector<std::string> kOot = build(
        "OOT_", { "RECOVERY_HEART", "MAGIC_JAR_SMALL", "MAGIC_JAR_LARGE", "FAIRY_BIG", "STICK", "STICKS_5", "STICKS_10",
                  "NUTS_5", "NUTS_5_ALT", "NUTS_10", "BOMB", "BOMBS_5", "BOMBS_10", "BOMBS_20", "BOMBS_30", "BOMBCHU_5",
                  "BOMBCHU_10", "BOMBCHU_20", "ARROWS_5", "ARROWS_10", "ARROWS_30", "DEKU_SEEDS_5", "DEKU_SEEDS_30",
                  "RUPEE_GREEN", "RUPEE_BLUE", "RUPEE_RED", "RUPEE_PURPLE", "RUPEE_HUGE", "POTION_RED", "POTION_GREEN",
                  "POTION_BLUE", "FISH", "BUGS", "BLUE_FIRE", "POE", "BIG_POE", "FAIRY", "MILK", "CHATEAU",
                  "SHIELD_DEKU", "SHIELD_HYLIAN" });
    static const std::vector<std::string> kMm = build(
        "MM_", { "RECOVERY_HEART", "MAGIC_JAR_SMALL", "MAGIC_JAR_LARGE", "FAIRY_BIG", "STICK", "NUT", "NUTS_5", "NUTS_10",
                 "BOMB", "BOMBS_5", "BOMBS_10", "BOMBS_20", "BOMBS_30", "BOMBCHU", "BOMBCHU_5", "BOMBCHU_10",
                 "BOMBCHU_20", "ARROWS_10", "ARROWS_30", "ARROWS_40", "DEKU_SEEDS_5", "DEKU_SEEDS_30", "RUPEE_GREEN",
                 "RUPEE_BLUE", "RUPEE_RED", "RUPEE_PURPLE", "RUPEE_SILVER", "RUPEE_GOLD", "MAGIC_BEAN", "POTION_RED",
                 "POTION_GREEN", "POTION_BLUE", "FISH", "BUGS", "BLUE_FIRE", "POE", "BIG_POE", "FAIRY", "MILK",
                 "CHATEAU", "SHIELD_DEKU", "SHIELD_HERO" });
    return game == OotmmGrantGame::Oot ? kOot : kMm;
}

std::string OotmmItemGrant::VitalFamily(std::string_view itemId) {
    const std::string_view base = SuffixOf(itemId);
    const OotmmGrantGame game = GameOf(itemId);
    if (game == OotmmGrantGame::Shared) {
        return std::string(base);
    }
    const auto& vital = VitalItems(game);
    if (std::find(vital.begin(), vital.end(), itemId) == vital.end()) {
        return std::string(base);
    }
    if (base == "RECOVERY_HEART") {
        return "health";
    }
    if (base.starts_with("MAGIC_JAR_")) {
        return "magic";
    }
    if (base == "FAIRY_BIG") {
        return "fairy";
    }
    if (base == "STICK" || base.starts_with("STICKS_")) {
        return "sticks";
    }
    if (base == "NUT" || base.starts_with("NUTS_")) {
        return "nuts";
    }
    if (base == "BOMB" || base.starts_with("BOMBS_")) {
        return "bombs";
    }
    if (base.starts_with("BOMBCHU")) {
        return "bombchu";
    }
    if (base.starts_with("ARROWS_")) {
        return "arrows";
    }
    if (base.starts_with("DEKU_SEEDS_")) {
        return "seeds";
    }
    if (base.starts_with("RUPEE_")) {
        return "rupees";
    }
    if (base.starts_with("POTION_") || base == "FISH" || base == "BUGS" || base == "BLUE_FIRE" || base == "POE" ||
        base == "BIG_POE" || base == "FAIRY" || base == "MILK" || base == "CHATEAU") {
        return "bottles";
    }
    if (base == "MAGIC_BEAN") {
        return "beans";
    }
    return std::string(base);
}

int32_t OotmmItemGrant::VitalUnit(std::string_view itemId, bool sharedMagic) {
    const std::string family = VitalFamily(itemId);
    const std::string_view base = SuffixOf(itemId);
    if (family == "health") {
        return 0x10;
    }
    if (family == "magic") {
        return ResolveOotmmMagicJarAmount(GameOf(itemId), sharedMagic, base == "MAGIC_JAR_LARGE", 1);
    }
    if (family == "rupees") {
        static constexpr std::pair<std::string_view, int32_t> kRupees[] = {
            { "RUPEE_GREEN", 1 },   { "RUPEE_BLUE", 5 },    { "RUPEE_RED", 20 },  { "RUPEE_PURPLE", 50 },
            { "RUPEE_SILVER", 100 }, { "RUPEE_GOLD", 200 }, { "RUPEE_HUGE", 200 },
        };
        for (const auto& [name, value] : kRupees) {
            if (base == name) {
                return value;
            }
        }
        return 1;
    }
    size_t start = 0;
    while (start <= base.size()) {
        const size_t end = std::min(base.find('_', start), base.size());
        const std::string_view token = base.substr(start, end - start);
        if (!token.empty() && std::all_of(token.begin(), token.end(), [](char c) { return c >= '0' && c <= '9'; })) {
            return std::stoi(std::string(token));
        }
        start = end + 1;
    }
    return 1;
}

std::string OotmmItemGrant::BuildVitals(OotmmGrantGame game, const std::function<int32_t(std::string_view)>& freeOf) {
    nlohmann::json vitals = nlohmann::json::object();
    for (const auto& itemId : VitalItems(game)) {
        const std::string family = VitalFamily(itemId);
        if (family != "fairy" && !vitals.contains(family)) {
            vitals[family] = std::max<int32_t>(0, freeOf(family));
        }
    }
    return vitals.dump();
}

static std::string ReadVitalsBody(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

static std::string VitalsStamp(const std::string& body) {
    uint64_t hash = 14695981039346656037ull;
    for (const unsigned char c : body) {
        hash = (hash ^ c) * 1099511628211ull;
    }
    char text[17];
    std::snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(hash));
    return text;
}

static nlohmann::json LoadVitalsUse(const std::string& vitalsPath, const std::string& stamp) {
    const auto use = nlohmann::json::parse(ReadVitalsBody(vitalsPath + ".used"), nullptr, false);
    if (use.is_object() && use.value("stamp", std::string()) == stamp && use.contains("used") &&
        use["used"].is_object()) {
        return use["used"];
    }
    return nlohmann::json::object();
}

std::optional<int32_t> OotmmItemGrant::RemoteRoom(std::string_view family, const std::string& vitalsPath) {
    const std::string body = ReadVitalsBody(vitalsPath);
    const auto vitals = nlohmann::json::parse(body, nullptr, false);
    const std::string key(family);
    if (!vitals.is_object() || !vitals.contains(key) || !vitals[key].is_number_integer()) {
        return std::nullopt;
    }
    const auto used = LoadVitalsUse(vitalsPath, VitalsStamp(body));
    const int64_t spent = used.contains(key) && used[key].is_number_integer() ? used[key].get<int64_t>() : 0;
    return static_cast<int32_t>(std::max<int64_t>(0, vitals[key].get<int64_t>() - spent));
}

void OotmmItemGrant::NoteRemoteGrant(std::string_view itemId, const std::string& vitalsPath, bool sharedMagic) {
    const std::string body = ReadVitalsBody(vitalsPath);
    if (body.empty() || !nlohmann::json::parse(body, nullptr, false).is_object()) {
        return;
    }
    const std::string stamp = VitalsStamp(body);
    auto used = LoadVitalsUse(vitalsPath, stamp);
    const auto spend = [&used](const std::string& family, int64_t amount) {
        const int64_t before = used.contains(family) && used[family].is_number_integer() ? used[family].get<int64_t>() : 0;
        used[family] = before + amount;
    };
    const std::string family = VitalFamily(itemId);
    if (family == "fairy") {
        spend("health", 1 << 20);
        spend("magic", 1 << 20);
    } else {
        spend(family, VitalUnit(itemId, sharedMagic));
    }
    WriteVitals(vitalsPath + ".used", nlohmann::json{ { "stamp", stamp }, { "used", used } }.dump());
}

void OotmmItemGrant::WriteVitals(const std::string& path, const std::string& body) {
    const std::string temp = path + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        out << body;
        if (!out) {
            return;
        }
    }
    std::error_code error;
    std::filesystem::rename(temp, path, error);
}

bool OotmmItemGrant::CanHoldRemote(std::string_view itemId, const OotmmInventory& inventory,
                                   const std::string& vitalsPath) {
    const bool oot = itemId.starts_with("OOT_");
    if (!oot && !itemId.starts_with("MM_")) {
        return true;
    }
    if (!ReadVitalsBody(vitalsPath).empty()) {
        const std::string family = VitalFamily(itemId);
        if (family == "fairy") {
            const auto health = RemoteRoom("health", vitalsPath);
            const auto magic = RemoteRoom("magic", vitalsPath);
            if (health || magic) {
                return health.value_or(0) > 0 || magic.value_or(0) > 0;
            }
        } else if (const auto room = RemoteRoom(family, vitalsPath)) {
            return *room > 0;
        }
    } else {
        const std::string_view unsaved = itemId.substr(oot ? 4 : 3);
        if (unsaved == "RECOVERY_HEART" || unsaved == "FAIRY_BIG" || unsaved.starts_with("MAGIC_JAR_") ||
            unsaved == "BOMB" || unsaved.starts_with("BOMBS_") || unsaved.starts_with("ARROWS_") ||
            unsaved.starts_with("DEKU_SEEDS_") || (!oot && unsaved.starts_with("SHIELD_"))) {
            return false;
        }
    }
    const std::string prefix = oot ? "OOT_" : "MM_";
    const std::string_view base = itemId.substr(prefix.size());
    const auto owned = [&](const char* name) {
        return inventory.Has(prefix + name) || inventory.Has(std::string("SHARED_") + name);
    };
    if (base == "BOMB" || base.starts_with("BOMBS_")) {
        return owned("BOMB_BAG");
    }
    if (base.starts_with("ARROWS_")) {
        return owned("BOW");
    }
    if (base.starts_with("DEKU_SEEDS_")) {
        return owned("SLINGSHOT");
    }
    if (base.starts_with("MAGIC_JAR_")) {
        return owned("MAGIC_UPGRADE");
    }
    if (base.starts_with("POTION_") || base == "FISH" || base == "BUGS" || base == "BLUE_FIRE" || base == "POE" ||
        base == "BIG_POE" || base == "FAIRY" || base == "MILK" || base == "CHATEAU") {
        for (const auto& [id, count] : inventory.GetValues()) {
            if (count != 0 && (id.starts_with(prefix + "BOTTLE") || id.starts_with("SHARED_BOTTLE"))) {
                return true;
            }
        }
        return false;
    }
    return true;
}

std::vector<OotmmGrantOp> OotmmItemGrant::Resolve(std::string_view itemId, uint32_t count) {
    std::vector<OotmmGrantOp> ops;
    if (count == 0 || itemId == "NOTHING" || itemId == "OOT_TRIFORCE_FULL" || itemId == "MM_MASK_MAJORA") {
        return ops;
    }

    const OotmmGrantGame game = GameOf(itemId);
    const std::string_view suffix = SuffixOf(itemId);

    if (suffix == "FAIRY_BIG") {
        ops.push_back(Op(OotmmGrantKind::BigFairy, game, suffix, count));
        return ops;
    }
    if (suffix == "BOMBCHU_BAG") {
        ops.push_back(Op(OotmmGrantKind::Equipment, game, suffix, count));
        return ops;
    }
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
    if (suffix == "BOMBCHU" || Contains(itemId, "_ARROWS_") || Contains(itemId, "_BOMBS_") ||
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
