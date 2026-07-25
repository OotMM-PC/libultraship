#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Ship {

enum class OotmmModelGame {
    Oot,
    Mm,
};

enum class OotmmRenderKind {
    OPA0,
    OPA01,
    OPA0_XLU1,
    XLU01,
    OPA1023,
    WALLET,
    BULLET_BAG,
    BOMBCHU_BAG,
    MAGIC_ARROW,
    POTION,
    COMPASS,
    SKULL_TOKEN,
    DEKU_NUTS,
    SMALL_RUPEE,
    OPA10_XLU32,
    HEART,
    FAIRY_BOTTLE,
    MOONS_TEAR,
    REMAINS,
    BOMBCHU_DL23,
    MUSIC_NOTE_ENV,
    OWL_STATUE,
    CLOCK,
    OPA10_XLU2,
    OPA0_DL26,
    OPA01_DL26,
    GORON_SWORD,
    MIRROR_SHIELD,
    SCALE,
    MAGIC_SPELL,
    FISH,
    POE,
    BLUE_FIRE,
    MUSIC_NOTE,
    JEWEL,
    MASTER_SWORD,
    KEY_RING,
    RUSTY_KEY,
    SKELETON_KEY,
    DOUBLE_DEFENSE,
    TRIFORCE,
    COIN,
    FAIRY_CONTAINER,
    STRAY_FAIRY,
    TRAP,
    POND_FISH,
};

struct OotmmRenderRecipe {
    OotmmModelGame SourceGame;
    OotmmRenderKind Kind;
    int32_t Param;
    std::array<std::string_view, 8> DisplayLists;
};

struct OotmmDebugPresentationItem {
    std::string_view Id;
    std::string_view Name;
    std::string_view Game;
};

class OotmmItemRenderCatalog {
  public:
    static const OotmmRenderRecipe* Find(std::string_view itemId, std::string_view itemName = {});
    static std::string CanonicalId(std::string_view itemId);
    static std::string ResourcePath(const OotmmRenderRecipe& recipe, size_t index, OotmmModelGame targetGame);
    static int32_t ResolveParam(const OotmmRenderRecipe& recipe, std::string_view itemName);
    static const std::vector<OotmmDebugPresentationItem>& DebugOnlyItems();
    static std::vector<std::string_view> AllIds();
};

} // namespace Ship
