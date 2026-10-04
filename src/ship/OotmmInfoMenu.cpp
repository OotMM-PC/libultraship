#include "libultraship/bridge/OotmmInfoMenu.h"

#include "libultraship/bridge/OotmmItemGrant.h"
#include "libultraship/bridge/OotmmProgress.h"

#include <algorithm>
#include <array>
#include <span>
#include <string>
#include <string_view>

namespace Ship {

namespace {

struct MenuEntry {
    const char* Name;
    const char* Id;
};

#include "OotmmInfoMenuTables.inc"

enum Screen {
    kScreenNone,
    kScreenInfo,
    kScreenNotesOot,
    kScreenNotesMm,
    kScreenSoulsOotEnemy,
    kScreenSoulsOotBoss,
    kScreenSoulsOotNpc,
    kScreenSoulsOotAnimal,
    kScreenSoulsOotMisc,
    kScreenSoulsMmEnemy,
    kScreenSoulsMmBoss,
    kScreenSoulsMmNpc,
    kScreenSoulsMmAnimal,
    kScreenSoulsMmMisc,
    kScreenRustyOot,
    kScreenRustyMm,
    kScreenCount,
};

enum InfoFlags : uint8_t {
    kOotRow = 0x00,
    kMmRow = 0x01,
    kMiscRow = 0x02,
    kMapCompass = 0x04,
    kBossKey = 0x08,
    kFairies = 0x10,
};

enum MiscRow {
    kTownFairy,
    kTokens,
    kTriforce,
    kCoinRed,
    kCoinGreen,
    kCoinBlue,
    kCoinYellow,
    kClocks,
};

struct InfoDef {
    const char* Name;
    const char* Code;
    const char* Dungeon;
    uint8_t Flags;
    int Index;
};

constexpr InfoDef kInfoOot[] = {
    { "Deku", "DT", "DT", kOotRow | kMapCompass, 0 },
    { "Dodongo", "DC", "DC", kOotRow | kMapCompass, 0 },
    { "Jabu", "JJ", "JJ", kOotRow | kMapCompass, 0 },
    { "Forest", "FOREST", "Forest", kOotRow | kMapCompass | kBossKey, 0 },
    { "Fire", "FIRE", "Fire", kOotRow | kMapCompass | kBossKey, 0 },
    { "Water", "WATER", "Water", kOotRow | kMapCompass | kBossKey, 0 },
    { "Shadow", "SHADOW", "Shadow", kOotRow | kMapCompass | kBossKey, 0 },
    { "Spirit", "SPIRIT", "Spirit", kOotRow | kMapCompass | kBossKey, 0 },
    { "Well", "BOTW", "BotW", kOotRow | kMapCompass, 0 },
    { "Ice", "IC", "IC", kOotRow | kMapCompass, 0 },
    { "Hideout", "GF", nullptr, kOotRow, 0 },
    { "GTG", "GTG", "GTG", kOotRow, 0 },
    { "Ganon", "GANON", "Ganon", kOotRow | kBossKey, 0 },
    { "Chest Game", "TCG", nullptr, kOotRow, 0 },
};

constexpr InfoDef kInfoMm[] = {
    { "Woodfall", "WF", "WF", kMmRow | kMapCompass | kBossKey | kFairies, 0 },
    { "Snowhead", "SH", "SH", kMmRow | kMapCompass | kBossKey | kFairies, 1 },
    { "Great Bay", "GB", "GB", kMmRow | kMapCompass | kBossKey | kFairies, 2 },
    { "Stone Tower", "ST", "ST", kMmRow | kMapCompass | kBossKey | kFairies, 3 },
    { "Clock Town", nullptr, nullptr, kMiscRow, kTownFairy },
};

constexpr InfoDef kInfoTokens = { "Tokens", nullptr, nullptr, kMiscRow, kTokens };
constexpr InfoDef kInfoTriforce = { "Triforce", nullptr, nullptr, kMiscRow, kTriforce };
constexpr InfoDef kInfoClocks = { "Clocks", nullptr, nullptr, kMiscRow, kClocks };
constexpr InfoDef kInfoCoins[] = {
    { "Red Coins", nullptr, nullptr, kMiscRow, kCoinRed },
    { "Green Coins", nullptr, nullptr, kMiscRow, kCoinGreen },
    { "Blue Coins", nullptr, nullptr, kMiscRow, kCoinBlue },
    { "Yellow Coins", nullptr, nullptr, kMiscRow, kCoinYellow },
};

constexpr std::array<const char*, 4> kCoinSettings = { "coinsRed", "coinsGreen", "coinsBlue", "coinsYellow" };
constexpr std::array<const char*, 4> kCoinItems = { "OOT_COIN_RED", "OOT_COIN_GREEN", "OOT_COIN_BLUE",
                                                    "OOT_COIN_YELLOW" };
constexpr std::array<uint32_t, 4> kCoinColors = { 0xFF0000FF, 0x00FF00FF, 0x0000FFFF, 0xFFFF00FF };
constexpr std::array<uint32_t, 5> kFairyColors = { 0xBA5084FF, 0x45852BFF, 0x7F65CCFF, 0xC2C164FF, 0xBC702DFF };

struct SongNote {
    const char* Name;
    const char* Suffix;
    const char* Setting;
};

constexpr SongNote kNotesOot[] = {
    { "Zelda's Lullaby", "ZELDA", nullptr },
    { "Epona's Song", "EPONA", nullptr },
    { "Saria's Song", "SARIA", nullptr },
    { "Sun's Song", "SUN", nullptr },
    { "Song of Time", "TIME", nullptr },
    { "Song of Storms", "STORMS", nullptr },
    { "Minuet of Forest", "TP_FOREST", nullptr },
    { "Bolero of Fire", "TP_FIRE", nullptr },
    { "Serenade of Water", "TP_WATER", nullptr },
    { "Nocturne of Shadow", "TP_SHADOW", nullptr },
    { "Requiem of Spirit", "TP_SPIRIT", nullptr },
    { "Prelude of Light", "TP_LIGHT", nullptr },
    { "Song of Healing", "HEALING", "songHealingOot" },
    { "Song of Soaring", "SOARING", "songSoaringOot" },
    { "Sonata of Awakening", "AWAKENING", "songAwakeningOot" },
    { "Goron Lullaby", "GORON", "songGoronOot" },
    { "New Wave Bossa Nova", "ZORA", "songZoraOot" },
    { "Elegy of Emptiness", "EMPTINESS", "elegyOot" },
    { "Oath to Order", "ORDER", "songOrderOot" },
};

constexpr SongNote kNotesMm[] = {
    { "Song of Time", "TIME", nullptr },
    { "Song of Healing", "HEALING", nullptr },
    { "Epona's Song", "EPONA", nullptr },
    { "Song of Soaring", "SOARING", nullptr },
    { "Song of Storms", "STORMS", nullptr },
    { "Sonata of Awakening", "AWAKENING", nullptr },
    { "Goron's Lullaby", "GORON", nullptr },
    { "New Wave Bossa Nova", "ZORA", nullptr },
    { "Elegy of Emptiness", "EMPTINESS", nullptr },
    { "Oath to Order", "ORDER", nullptr },
    { "Zelda's Lullaby", "ZELDA", "songZeldaLullabyMm" },
    { "Saria's Song", "SARIA", "songSariasMm" },
    { "Sun's Song", "SUN", "songSunMm" },
    { "Minuet of Forest", "TP_FOREST", "songMinuetMm" },
    { "Bolero of Fire", "TP_FIRE", "songBoleroMm" },
    { "Serenade of Water", "TP_WATER", "songSerenadeMm" },
    { "Requiem of Spirit", "TP_SPIRIT", "songRequiemMm" },
    { "Nocturne of Shadow", "TP_SHADOW", "songNocturneMm" },
    { "Prelude of Light", "TP_LIGHT", "songPreludeMm" },
};

struct SoulScreen {
    const char* Title;
    std::span<const MenuEntry> Entries;
    const char* Prefix;
    const char* Setting;
    const char* SharedSetting;
};

constexpr SoulScreen kSoulScreens[] = {
    { "OoT Enemy Souls", kSoulsEnemyOot, "OOT_SOUL_", "soulsEnemyOot", "sharedSoulsEnemy" },
    { "OoT Boss Souls", kSoulsBossOot, "OOT_SOUL_", "soulsBossOot", nullptr },
    { "OoT NPC Souls", kSoulsNpcOot, "OOT_SOUL_", "soulsNpcOot", "sharedSoulsNpc" },
    { "OoT Animal Souls", kSoulsAnimalOot, "OOT_SOUL_", "soulsAnimalOot", "sharedSoulsAnimal" },
    { "OoT Misc. Souls", kSoulsMiscOot, "OOT_SOUL_", "soulsMiscOot", "sharedSoulsMisc" },
    { "MM Enemy Souls", kSoulsEnemyMm, "MM_SOUL_", "soulsEnemyMm", "sharedSoulsEnemy" },
    { "MM Boss Souls", kSoulsBossMm, "MM_SOUL_", "soulsBossMm", nullptr },
    { "MM NPC Souls", kSoulsNpcMm, "MM_SOUL_", "soulsNpcMm", "sharedSoulsNpc" },
    { "MM Animal Souls", kSoulsAnimalMm, "MM_SOUL_", "soulsAnimalMm", "sharedSoulsAnimal" },
    { "MM Misc. Souls", kSoulsMiscMm, "MM_SOUL_", "soulsMiscMm", "sharedSoulsMisc" },
};

constexpr uint32_t kWhite = 0xFFFFFFFF;
constexpr uint32_t kTitle = 0xFFFF00FF;
constexpr uint32_t kMissing = 0x464646FF;
constexpr uint32_t kEmpty = 0x7F7F7FFF;
constexpr uint32_t kFull = 0xC86464FF;
constexpr uint32_t kPreCompleted = 0x3F3F3FFF;
constexpr uint32_t kDim = 0xFFFFFF46;

bool OnlyOot(const OotmmGameState& state) {
    return state.GetStringSetting("games", "ootmm") == "oot";
}

bool OnlyMm(const OotmmGameState& state) {
    return state.GetStringSetting("games", "ootmm") == "mm";
}

std::vector<const InfoDef*> InfoDefs(const OotmmGameState& state) {
    std::vector<const InfoDef*> defs;
    if (!OnlyMm(state)) {
        for (const InfoDef& def : kInfoOot) {
            defs.push_back(&def);
        }
    }
    if (!OnlyOot(state)) {
        for (const InfoDef& def : kInfoMm) {
            defs.push_back(&def);
        }
    }
    defs.push_back(&kInfoTokens);
    if (state.GetBoolSetting("clocks", false)) {
        defs.push_back(&kInfoClocks);
    }
    const std::string goal = state.GetStringSetting("goal", "both");
    if (goal == "triforce" || goal == "triforce3") {
        defs.push_back(&kInfoTriforce);
    }
    for (size_t i = 0; i < kCoinSettings.size(); i++) {
        if (state.GetIntSetting(kCoinSettings[i], 0) > 0) {
            defs.push_back(&kInfoCoins[i]);
        }
    }
    return defs;
}

std::vector<const SongNote*> Notes(const OotmmGameState& state, std::span<const SongNote> notes) {
    std::vector<const SongNote*> result;
    for (const SongNote& note : notes) {
        if (note.Setting == nullptr || state.GetBoolSetting(note.Setting, false)) {
            result.push_back(&note);
        }
    }
    return result;
}

bool ScreenEnabled(const OotmmGameState& state, int screen) {
    const bool notes = state.GetStringSetting("songs", "songLocations") == "notes";
    switch (screen) {
        case kScreenNotesOot:
            return notes && !OnlyMm(state);
        case kScreenNotesMm:
            return notes && !OnlyOot(state);
        case kScreenRustyOot:
            return state.GetBoolSetting("rustyKeysOot", false);
        case kScreenRustyMm:
            return state.GetBoolSetting("rustyKeysMm", false);
        default:
            if (screen >= kScreenSoulsOotEnemy && screen <= kScreenSoulsMmMisc) {
                return state.GetBoolSetting(kSoulScreens[screen - kScreenSoulsOotEnemy].Setting, false);
            }
            return true;
    }
}

int LineCount(const OotmmGameState& state, int screen) {
    switch (screen) {
        case kScreenInfo:
            return static_cast<int>(InfoDefs(state).size());
        case kScreenNotesOot:
            return static_cast<int>(Notes(state, kNotesOot).size());
        case kScreenNotesMm:
            return static_cast<int>(Notes(state, kNotesMm).size());
        case kScreenRustyOot:
            return static_cast<int>(std::size(kRustyKeysOot));
        case kScreenRustyMm:
            return static_cast<int>(std::size(kRustyKeysMm));
        default:
            if (screen >= kScreenSoulsOotEnemy && screen <= kScreenSoulsMmMisc) {
                return static_cast<int>(kSoulScreens[screen - kScreenSoulsOotEnemy].Entries.size());
            }
            return 0;
    }
}

int DigitCount(uint32_t value) {
    return value >= 100 ? 3 : (value >= 10 ? 2 : 1);
}

uint32_t CountColor(uint32_t count, uint32_t max) {
    if (count == 0) {
        return kEmpty;
    }
    return count >= max ? kFull : kWhite;
}

class Painter {
  public:
    explicit Painter(std::vector<OotmmMenuQuad>& quads) : mQuads(quads) {
    }

    void Char(char glyph, float x, float y, uint32_t color) {
        if (glyph > ' ' && glyph < 0x7F) {
            mQuads.push_back({ false, glyph, OotmmMenuIcon::Key, x, y, color });
        }
    }

    void Text(std::string_view text, float x, float y, uint32_t color) {
        for (size_t i = 0; i < text.size(); i++) {
            Char(text[i], x + OotmmInfoMenu::kGlyphWidth * static_cast<float>(i + 1), y, color);
        }
    }

    void Icon(OotmmMenuIcon icon, float x, float y, uint32_t color) {
        mQuads.push_back({ true, ' ', icon, x, y, color });
    }

    void Number(uint32_t value, uint32_t max, int digits, float x, float y, uint32_t color, bool showMax) {
        if (value == 0) {
            x += OotmmInfoMenu::kGlyphWidth * static_cast<float>(digits - 1);
            Char('0', x, y, color);
            x += OotmmInfoMenu::kGlyphWidth;
        } else {
            uint32_t divisor = 1;
            for (int i = 1; i < digits; i++) {
                divisor *= 10;
            }
            bool started = false;
            while (divisor != 0) {
                const uint32_t digit = (value / divisor) % 10;
                if (digit != 0 || started) {
                    Char(static_cast<char>('0' + digit), x, y, color);
                    started = true;
                }
                divisor /= 10;
                x += OotmmInfoMenu::kGlyphWidth;
            }
        }
        if (showMax) {
            Char('/', x, y, color);
            Number(max, max, digits, x + OotmmInfoMenu::kGlyphWidth, y, color, false);
        }
    }

    void Counter(uint32_t value, uint32_t max, int digits, float x, float y, bool showMax) {
        Number(value, max, digits, x, y, CountColor(value, max), showMax);
    }

  private:
    std::vector<OotmmMenuQuad>& mQuads;
};

uint32_t Collected(const OotmmInventory& inventory, std::string_view id) {
    return inventory.Count(std::string(id));
}

uint32_t SmallKeys(const OotmmGameState& state, const OotmmInventory& inventory, OotmmGame game,
                   std::string_view code) {
    const uint32_t max = OotmmMaxSmallKeys(state, game, code);
    const std::string prefix = game == OotmmGame::Oot ? "OOT_" : "MM_";
    const std::string suffix(code);
    if (inventory.Has(prefix + "SKELETON_KEY") || inventory.Has("SHARED_SKELETON_KEY") ||
        inventory.Has(prefix + "KEY_RING_" + suffix)) {
        return max;
    }
    return std::min(max, inventory.Count(prefix + "SMALL_KEY_" + suffix));
}

void SilverRupees(Painter& paint, const OotmmGameState& state, const OotmmInventory& inventory, std::string_view code,
                  float x, float y) {
    const bool mq = OotmmIsMasterQuest(state, code);
    const std::string prefix = std::string(code) + "_";
    bool first = true;
    for (const std::string_view suffix : OotmmSilverRupees::All()) {
        if (suffix != code && !suffix.starts_with(prefix)) {
            continue;
        }
        const uint32_t required = OotmmSilverRupees::Required(suffix, mq);
        if (required == 0) {
            continue;
        }
        if (first) {
            paint.Icon(OotmmMenuIcon::Rupee, x, y, kWhite);
            x += OotmmInfoMenu::kIconSize;
            first = false;
        }
        const std::string name(suffix);
        const uint32_t count = inventory.Has("OOT_POUCH_SILVER_" + name)
                                   ? required
                                   : std::min(required, inventory.Count("OOT_RUPEE_SILVER_" + name));
        const int digits = DigitCount(required);
        paint.Counter(count, required, digits, x, y, false);
        x += OotmmInfoMenu::kGlyphWidth * static_cast<float>(digits) + 3.0f;
    }
}

void MoonSun(Painter& paint, uint32_t mask, float x, float y, bool night) {
    paint.Icon(night ? OotmmMenuIcon::Moon : OotmmMenuIcon::Sun, x, y, kWhite);
    for (int i = 0; i < 3; i++) {
        const int bit = (night ? 1 : 0) + 2 * i;
        paint.Char(static_cast<char>('1' + i), x + 12.0f + 8.0f * static_cast<float>(i), y,
                   (mask & (1u << bit)) != 0 ? kWhite : kDim);
    }
}

void MiscLine(Painter& paint, const OotmmGameState& state, const OotmmInventory& inventory,
              const OotmmMenuTriforce& triforce, const InfoDef& def, float x, float y) {
    switch (def.Index) {
        case kTownFairy:
            paint.Icon(OotmmMenuIcon::Fairy, x + 174.0f, y, kFairyColors[4]);
            paint.Counter(inventory.Has("MM_STRAY_FAIRY_TOWN") ? 1 : 0, 1, 2, x + 186.0f, y, false);
            break;
        case kTokens:
            if (!OnlyMm(state)) {
                paint.Icon(OotmmMenuIcon::Skull, x + 104.0f, y, 0xFFFF00FF);
                paint.Counter(Collected(inventory, "OOT_GS_TOKEN"), 100, 3, x + 116.0f, y, false);
            }
            if (!OnlyOot(state)) {
                paint.Icon(OotmmMenuIcon::Skull, x + 144.0f, y, 0x00FF00FF);
                paint.Counter(Collected(inventory, "MM_GS_TOKEN_SWAMP"), 30, 2, x + 156.0f, y, false);
                paint.Icon(OotmmMenuIcon::Skull, x + 184.0f, y, 0x0000FFFF);
                paint.Counter(Collected(inventory, "MM_GS_TOKEN_OCEAN"), 30, 2, x + 196.0f, y, false);
            }
            break;
        case kTriforce:
            paint.Icon(OotmmMenuIcon::Triforce, x + 104.0f, y, kWhite);
            paint.Counter(triforce.Count, triforce.Max, DigitCount(triforce.Max), x + 116.0f, y, true);
            break;
        case kCoinRed:
        case kCoinGreen:
        case kCoinBlue:
        case kCoinYellow: {
            const int coin = def.Index - kCoinRed;
            const uint32_t max = static_cast<uint32_t>(state.GetIntSetting(kCoinSettings[coin], 0));
            paint.Icon(OotmmMenuIcon::Coin, x + 104.0f, y, kCoinColors[coin]);
            paint.Counter(Collected(inventory, kCoinItems[coin]), max, DigitCount(max), x + 116.0f, y, true);
            break;
        }
        case kClocks: {
            const uint32_t mask = OotmmClockMask(state, inventory);
            MoonSun(paint, mask, x + 110.0f, y, false);
            MoonSun(paint, mask, x + 170.0f, y, true);
            break;
        }
        default:
            break;
    }
}

void DungeonLine(Painter& paint, const OotmmGameState& state, const OotmmInventory& inventory, const InfoDef& def,
                 float x, float y) {
    const bool mm = (def.Flags & kMmRow) != 0;
    const OotmmGame game = mm ? OotmmGame::Mm : OotmmGame::Oot;
    const std::string prefix = mm ? "MM_" : "OOT_";
    const std::string code = def.Code;
    if ((def.Flags & kMapCompass) != 0) {
        if (inventory.Has(prefix + "MAP_" + code)) {
            paint.Icon(OotmmMenuIcon::Map, x + 104.0f, y, kWhite);
        }
        if (inventory.Has(prefix + "COMPASS_" + code)) {
            paint.Icon(OotmmMenuIcon::Compass, x + 116.0f, y, kWhite);
        }
    }
    if ((def.Flags & kBossKey) != 0 && inventory.Has(prefix + "BOSS_KEY_" + code)) {
        paint.Icon(OotmmMenuIcon::BossKey, x + 128.0f, y, kWhite);
    }
    const uint32_t maxKeys = OotmmMaxSmallKeys(state, game, code);
    if (maxKeys != 0) {
        paint.Icon(OotmmMenuIcon::Key, x + 144.0f, y, kWhite);
        paint.Counter(SmallKeys(state, inventory, game, code), maxKeys, 1, x + 156.0f, y, false);
    }
    if ((def.Flags & kFairies) != 0) {
        const uint32_t reward = static_cast<uint32_t>(state.GetIntSetting("strayFairyRewardCount", 15));
        paint.Icon(OotmmMenuIcon::Fairy, x + 174.0f, y, kFairyColors[def.Index]);
        paint.Counter(Collected(inventory, "MM_STRAY_FAIRY_" + code), reward, 2, x + 186.0f, y, false);
    }
    if (!mm && state.GetStringSetting("silverRupeeShuffle", "vanilla") != "vanilla") {
        SilverRupees(paint, state, inventory, code, x + 170.0f, y);
    }
}

void InfoLine(Painter& paint, const OotmmGameState& state, const OotmmInventory& inventory,
              const OotmmMenuTriforce& triforce, const InfoDef& def, float y) {
    const float x = OotmmInfoMenu::kLeft;
    const bool preCompleted = def.Dungeon != nullptr && state.IsDungeonPreCompleted(def.Dungeon);
    paint.Text(def.Name, x, y, preCompleted ? kPreCompleted : kWhite);
    if ((def.Flags & kMiscRow) != 0) {
        MiscLine(paint, state, inventory, triforce, def, x, y);
    } else {
        DungeonLine(paint, state, inventory, def, x, y);
    }
}

bool OwnsSoul(const OotmmGameState& state, const OotmmInventory& inventory, const SoulScreen& screen,
              const MenuEntry& entry) {
    if (inventory.Has(std::string(screen.Prefix) + entry.Id)) {
        return true;
    }
    return screen.SharedSetting != nullptr && state.GetBoolSetting(screen.SharedSetting, false) &&
           inventory.Has(std::string("SHARED_SOUL_") + entry.Id);
}

uint32_t NoteCount(const OotmmInventory& inventory, bool mm, const SongNote& note) {
    const std::string suffix = note.Suffix;
    return inventory.Count((mm ? "MM_SONG_NOTE_" : "OOT_SONG_NOTE_") + suffix) +
           inventory.Count("SHARED_SONG_NOTE_" + suffix);
}

uint32_t NoteMax(bool mm, const SongNote& note) {
    const std::string id = (mm ? "MM_SONG_NOTE_" : "OOT_SONG_NOTE_") + std::string(note.Suffix);
    const auto definition = OotmmItemCatalog::Describe(id, note.Name);
    return definition.has_value() ? definition->MaxValue : 6;
}

float LineY(int line) {
    return 42.0f - 12.0f * static_cast<float>(line);
}

} // namespace

const char* OotmmInfoMenu::GlyphTexture(char glyph) {
    static std::array<std::string, 96> sPaths;
    if (glyph < ' ' || glyph >= 0x7F) {
        return nullptr;
    }
    std::string& path = sPaths[glyph - ' '];
    if (path.empty()) {
        static constexpr char kHex[] = "0123456789ABCDEF";
        path = "__OTR__objects/ootmm_menu/gMenuGlyph";
        path += kHex[(glyph >> 4) & 0xF];
        path += kHex[glyph & 0xF];
        path += "Tex";
    }
    return path.c_str();
}

const char* OotmmInfoMenu::IconTexture(OotmmMenuIcon icon) {
    switch (icon) {
        case OotmmMenuIcon::Key:
            return "__OTR__objects/ootmm_menu/gMenuIconKeyTex";
        case OotmmMenuIcon::BossKey:
            return "__OTR__objects/ootmm_menu/gMenuIconBossKeyTex";
        case OotmmMenuIcon::Map:
            return "__OTR__objects/ootmm_menu/gMenuIconMapTex";
        case OotmmMenuIcon::Compass:
            return "__OTR__objects/ootmm_menu/gMenuIconCompassTex";
        case OotmmMenuIcon::Fairy:
            return "__OTR__objects/ootmm_menu/gMenuIconFairyTex";
        case OotmmMenuIcon::Skull:
            return "__OTR__objects/ootmm_menu/gMenuIconSkullTex";
        case OotmmMenuIcon::Triforce:
            return "__OTR__objects/ootmm_menu/gMenuIconTriforceTex";
        case OotmmMenuIcon::Rupee:
            return "__OTR__objects/ootmm_menu/gMenuIconRupeeTex";
        case OotmmMenuIcon::Coin:
            return "__OTR__objects/ootmm_menu/gMenuIconCoinTex";
        case OotmmMenuIcon::Sun:
            return "__OTR__objects/ootmm_menu/gMenuIconSunTex";
        case OotmmMenuIcon::Moon:
            return "__OTR__objects/ootmm_menu/gMenuIconMoonTex";
    }
    return nullptr;
}

bool OotmmInfoMenu::IsOpen() const {
    return mScreen != kScreenNone;
}

int OotmmInfoMenu::Screen() const {
    return mScreen;
}

int OotmmInfoMenu::Cursor() const {
    return mCursor;
}

bool OotmmInfoMenu::Next(const OotmmGameState& state, const OotmmInventory& inventory) {
    if (state.GetBoolSetting("menuNotebook", false) && !inventory.Has("MM_BOMBER_NOTEBOOK")) {
        return false;
    }
    mCursor = 0;
    do {
        mScreen++;
    } while (mScreen < kScreenCount && !ScreenEnabled(state, mScreen));
    if (mScreen >= kScreenCount) {
        mScreen = kScreenNone;
    }
    return true;
}

bool OotmmInfoMenu::Scroll(const OotmmGameState& state, float stickY) {
    if (mDelay > 0) {
        mDelay--;
        return false;
    }
    const int last = std::max(0, LineCount(state, mScreen) - kLines);
    mCursor = std::min(mCursor, last);
    bool moved = false;
    if (stickY > 0.5f && mCursor > 0) {
        mCursor--;
        moved = true;
    }
    if (stickY < -0.5f && mCursor < last) {
        mCursor++;
        moved = true;
    }
    if (moved) {
        mDelay = 3;
    }
    return moved;
}

std::vector<OotmmMenuQuad> OotmmInfoMenu::Layout(const OotmmGameState& state, const OotmmInventory& inventory,
                                                 const OotmmMenuTriforce& triforce) const {
    std::vector<OotmmMenuQuad> quads;
    Painter paint(quads);
    const int count = LineCount(state, mScreen);
    const int first = std::clamp(mCursor, 0, std::max(0, count - kLines));
    const int shown = std::min(kLines, count - first);
    const float titleY = 54.0f;

    switch (mScreen) {
        case kScreenInfo: {
            paint.Text("Information", kLeft, titleY, kTitle);
            const auto defs = InfoDefs(state);
            for (int i = 0; i < shown; i++) {
                InfoLine(paint, state, inventory, triforce, *defs[first + i], LineY(i));
            }
            break;
        }
        case kScreenNotesOot:
        case kScreenNotesMm: {
            const bool mm = mScreen == kScreenNotesMm;
            paint.Text(mm ? "MM Song Notes" : "OoT Song Notes", kLeft, titleY, kTitle);
            const auto notes =
                Notes(state, mm ? std::span<const SongNote>(kNotesMm) : std::span<const SongNote>(kNotesOot));
            for (int i = 0; i < shown; i++) {
                const SongNote& note = *notes[first + i];
                paint.Text(note.Name, kLeft, LineY(i), kWhite);
                paint.Counter(NoteCount(inventory, mm, note), NoteMax(mm, note), 1, kLeft + kGlyphWidth * 24.0f,
                              LineY(i), false);
            }
            break;
        }
        case kScreenRustyOot:
        case kScreenRustyMm: {
            const bool mm = mScreen == kScreenRustyMm;
            paint.Text(mm ? "MM Rusty Keys" : "OoT Rusty Keys", kLeft, titleY, kTitle);
            const std::span<const MenuEntry> keys =
                mm ? std::span<const MenuEntry>(kRustyKeysMm) : std::span<const MenuEntry>(kRustyKeysOot);
            for (int i = 0; i < shown; i++) {
                const MenuEntry& key = keys[first + i];
                paint.Text(key.Name, kLeft, LineY(i), inventory.Has(key.Id) ? kWhite : kMissing);
            }
            break;
        }
        default:
            if (mScreen >= kScreenSoulsOotEnemy && mScreen <= kScreenSoulsMmMisc) {
                const SoulScreen& screen = kSoulScreens[mScreen - kScreenSoulsOotEnemy];
                paint.Text(screen.Title, kLeft, titleY, kTitle);
                for (int i = 0; i < shown; i++) {
                    const MenuEntry& soul = screen.Entries[first + i];
                    paint.Text(soul.Name, kLeft, LineY(i),
                               OwnsSoul(state, inventory, screen, soul) ? kWhite : kMissing);
                }
            }
            break;
    }
    return quads;
}

} // namespace Ship
