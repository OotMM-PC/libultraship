#include "libultraship/bridge/OotmmItemRender.h"

#include <algorithm>
#include <unordered_map>

namespace Ship {

namespace {

const std::unordered_map<std::string_view, OotmmRenderRecipe> kRecipes = {
#include "OotmmItemRenderCatalog.inc"
};

const OotmmRenderRecipe kRustyKeyRecipe = {
    OotmmModelGame::Oot,
    OotmmRenderKind::RUSTY_KEY,
    0,
    { "objects/object_key/gSmallKeyCustomDL", "", "", "", "", "", "", "" },
};

const std::array<OotmmRenderRecipe, 6> kStrayFairyRecipes = {{
    { OotmmModelGame::Mm, OotmmRenderKind::STRAY_FAIRY, 1,
      { "objects/gameplay_keep/gStrayFairySkel", "objects/gameplay_keep/gStrayFairyFlyingAnim" } },
    { OotmmModelGame::Mm, OotmmRenderKind::STRAY_FAIRY, 2,
      { "objects/gameplay_keep/gStrayFairySkel", "objects/gameplay_keep/gStrayFairyFlyingAnim" } },
    { OotmmModelGame::Mm, OotmmRenderKind::STRAY_FAIRY, 3,
      { "objects/gameplay_keep/gStrayFairySkel", "objects/gameplay_keep/gStrayFairyFlyingAnim" } },
    { OotmmModelGame::Mm, OotmmRenderKind::STRAY_FAIRY, 4,
      { "objects/gameplay_keep/gStrayFairySkel", "objects/gameplay_keep/gStrayFairyFlyingAnim" } },
    { OotmmModelGame::Mm, OotmmRenderKind::STRAY_FAIRY, 5,
      { "objects/gameplay_keep/gStrayFairySkel", "objects/gameplay_keep/gStrayFairyFlyingAnim" } },
    { OotmmModelGame::Mm, OotmmRenderKind::STRAY_FAIRY, 6,
      { "objects/gameplay_keep/gStrayFairySkel", "objects/gameplay_keep/gStrayFairyFlyingAnim" } },
}};

const std::array<OotmmRenderRecipe, 2> kPondFishRecipes = {{
    { OotmmModelGame::Oot, OotmmRenderKind::POND_FISH, 0,
      { "objects/object_fish/gFishingFishSkel", "objects/object_fish/gFishingFishAnim" } },
    { OotmmModelGame::Oot, OotmmRenderKind::POND_FISH, 1,
      { "objects/object_fish/gFishingLoachSkel", "objects/object_fish/gFishingLoachAnim" } },
}};

const std::unordered_map<std::string_view, OotmmRenderRecipe> kTrapRecipes = {
    { "OOT_TRAP_ICE", { OotmmModelGame::Oot, OotmmRenderKind::TRAP, 0, {} } },
    { "OOT_TRAP_FIRE", { OotmmModelGame::Oot, OotmmRenderKind::TRAP, 1, {} } },
    { "OOT_TRAP_SHOCK", { OotmmModelGame::Oot, OotmmRenderKind::TRAP, 2, {} } },
    { "OOT_TRAP_DRAIN",
      { OotmmModelGame::Oot, OotmmRenderKind::TRAP, 3,
        { "objects/object_gi_heart/gGiRecoveryHeartDL" } } },
    { "OOT_TRAP_ANTI_MAGIC",
      { OotmmModelGame::Oot, OotmmRenderKind::TRAP, 4,
        { "objects/object_gi_magicpot/gGiMagicJarSmallDL" } } },
    { "OOT_TRAP_KNOCKBACK",
      { OotmmModelGame::Oot, OotmmRenderKind::TRAP, 5,
        { "objects/object_trap/gSlidingBladeTrapDL" } } },
    { "OOT_TRAP_RUPOOR", { OotmmModelGame::Oot, OotmmRenderKind::TRAP, 6, {} } },
};

const OotmmRenderRecipe kFairyOcarinaRecipe = {
    OotmmModelGame::Oot, OotmmRenderKind::OPA0_XLU1, 0,
    { "objects/object_gi_ocarina_0/gGiOcarinaFairyDL",
      "objects/object_gi_ocarina_0/gGiOcarinaFairyHolesDL" },
};

const std::array<OotmmRenderRecipe, 2> kBombBagUpgradeRecipes = {{
    { OotmmModelGame::Oot, OotmmRenderKind::OPA1023, 0,
      { "objects/object_gi_bombpouch/gGiBombBagDL",
        "objects/object_gi_bombpouch/gGiBombBag30BagColorDL",
        "objects/object_gi_bombpouch/gGiBombBag30RingColorDL",
        "objects/object_gi_bombpouch/gGiBombBagRingDL" } },
    { OotmmModelGame::Oot, OotmmRenderKind::OPA1023, 0,
      { "objects/object_gi_bombpouch/gGiBombBagDL",
        "objects/object_gi_bombpouch/gGiBombBag40BagColorDL",
        "objects/object_gi_bombpouch/gGiBombBag40RingColorDL",
        "objects/object_gi_bombpouch/gGiBombBagRingDL" } },
}};

const std::array<OotmmRenderRecipe, 3> kBombchuBagRecipes = {{
    { OotmmModelGame::Oot, OotmmRenderKind::BOMBCHU_BAG, 0,
      { "objects/object_gi_bombpouch/gGiBombBagDL",
        "objects/object_gi_bombpouch/gGiBombBag20BagColorDL",
        "objects/object_gi_bombpouch/gGiBombBag20RingColorDL",
        "objects/object_gi_bombpouch/gGiBombBagRingDL",
        "objects/object_gi_bomb_2/gGiBombchuDL" } },
    { OotmmModelGame::Oot, OotmmRenderKind::BOMBCHU_BAG, 0,
      { "objects/object_gi_bombpouch/gGiBombBagDL",
        "objects/object_gi_bombpouch/gGiBombBag30BagColorDL",
        "objects/object_gi_bombpouch/gGiBombBag30RingColorDL",
        "objects/object_gi_bombpouch/gGiBombBagRingDL",
        "objects/object_gi_bomb_2/gGiBombchuDL" } },
    { OotmmModelGame::Oot, OotmmRenderKind::BOMBCHU_BAG, 0,
      { "objects/object_gi_bombpouch/gGiBombBagDL",
        "objects/object_gi_bombpouch/gGiBombBag40BagColorDL",
        "objects/object_gi_bombpouch/gGiBombBag40RingColorDL",
        "objects/object_gi_bombpouch/gGiBombBagRingDL",
        "objects/object_gi_bomb_2/gGiBombchuDL" } },
}};

const std::array<OotmmRenderRecipe, 2> kQuiverUpgradeRecipes = {{
    { OotmmModelGame::Oot, OotmmRenderKind::OPA1023, 0,
      { "objects/object_gi_arrowcase/gGiQuiverInnerDL",
        "objects/object_gi_arrowcase/gGiQuiver40InnerColorDL",
        "objects/object_gi_arrowcase/gGiQuiver40OuterColorDL",
        "objects/object_gi_arrowcase/gGiQuiverOuterDL" } },
    { OotmmModelGame::Oot, OotmmRenderKind::OPA1023, 0,
      { "objects/object_gi_arrowcase/gGiQuiverInnerDL",
        "objects/object_gi_arrowcase/gGiQuiver50InnerColorDL",
        "objects/object_gi_arrowcase/gGiQuiver50OuterColorDL",
        "objects/object_gi_arrowcase/gGiQuiverOuterDL" } },
}};

const std::array<OotmmRenderRecipe, 2> kBulletBagUpgradeRecipes = {{
    { OotmmModelGame::Oot, OotmmRenderKind::BULLET_BAG, 0,
      { "objects/object_gi_dekupouch/gGiBulletBagDL",
        "objects/object_gi_dekupouch/gGiBulletBagColorDL",
        "objects/object_gi_dekupouch/gGiBulletBagStringDL",
        "objects/object_gi_dekupouch/gGiBulletBagStringColorDL",
        "objects/object_gi_dekupouch/gGiBulletBagWritingDL" } },
    { OotmmModelGame::Oot, OotmmRenderKind::BULLET_BAG, 0,
      { "objects/object_gi_dekupouch/gGiBulletBagDL",
        "objects/object_gi_dekupouch/gGiBulletBag50ColorDL",
        "objects/object_gi_dekupouch/gGiBulletBagStringDL",
        "objects/object_gi_dekupouch/gGiBulletBag50StringColorDL",
        "objects/object_gi_dekupouch/gGiBulletBagWritingDL" } },
}};

const OotmmRenderRecipe kLongshotRecipe = {
    OotmmModelGame::Oot, OotmmRenderKind::OPA0, 0,
    { "objects/object_gi_hookshot/gGiLongshotDL" },
};

const OotmmRenderRecipe kLargeMagicRecipe = {
    OotmmModelGame::Oot, OotmmRenderKind::OPA0, 0,
    { "objects/object_gi_magicpot/gGiMagicJarLargeDL" },
};

const std::array<OotmmRenderRecipe, 2> kGauntletRecipes = {{
    { OotmmModelGame::Oot, OotmmRenderKind::OPA10_XLU32, 0,
      { "objects/object_gi_gloves/gGiGauntletsDL",
        "objects/object_gi_gloves/gGiSilverGauntletsColorDL",
        "objects/object_gi_gloves/gGiGauntletsPlateDL",
        "objects/object_gi_gloves/gGiSilverGauntletsPlateColorDL" } },
    { OotmmModelGame::Oot, OotmmRenderKind::OPA10_XLU32, 0,
      { "objects/object_gi_gloves/gGiGauntletsDL",
        "objects/object_gi_gloves/gGiGoldenGauntletsColorDL",
        "objects/object_gi_gloves/gGiGauntletsPlateDL",
        "objects/object_gi_gloves/gGiGoldenGauntletsPlateColorDL" } },
}};

const OotmmRenderRecipe kGiantWalletRecipe = {
    OotmmModelGame::Oot, OotmmRenderKind::WALLET, 0,
    { "objects/object_gi_purse/gGiWalletDL",
      "objects/object_gi_purse/gGiGiantsWalletColorDL",
      "objects/object_gi_purse/gGiGiantsWalletRupeeOuterColorDL",
      "objects/object_gi_purse/gGiWalletRupeeOuterDL",
      "objects/object_gi_purse/gGiGiantsWalletStringColorDL",
      "objects/object_gi_purse/gGiWalletStringDL",
      "objects/object_gi_purse/gGiGiantsWalletRupeeInnerColorDL",
      "objects/object_gi_purse/gGiWalletRupeeInnerDL" },
};

const OotmmRenderRecipe* ProgressiveRecipe(
    std::string_view canonicalId, std::string_view itemName) {
    if (itemName.empty()) {
        return nullptr;
    }
    if (canonicalId.ends_with("_OCARINA") &&
        itemName.find("Fairy Ocarina") != std::string_view::npos) {
        return &kFairyOcarinaRecipe;
    }
    if (canonicalId.ends_with("_BOMB_BAG")) {
        if (itemName.find("Biggest Bomb Bag") != std::string_view::npos) {
            return &kBombBagUpgradeRecipes[1];
        }
        if (itemName.find("Big Bomb Bag") != std::string_view::npos) {
            return &kBombBagUpgradeRecipes[0];
        }
    }
    if (canonicalId.ends_with("_BOMBCHU_BAG")) {
        if (itemName.find("Biggest Bombchu Bag") != std::string_view::npos) {
            return &kBombchuBagRecipes[2];
        }
        if (itemName.find("Big Bombchu Bag") != std::string_view::npos) {
            return &kBombchuBagRecipes[1];
        }
        return &kBombchuBagRecipes[0];
    }
    if (canonicalId.ends_with("_BOW")) {
        if (itemName.find("Biggest Quiver") != std::string_view::npos) {
            return &kQuiverUpgradeRecipes[1];
        }
        if (itemName.find("Big Quiver") != std::string_view::npos) {
            return &kQuiverUpgradeRecipes[0];
        }
    }
    if (canonicalId.ends_with("_SLINGSHOT")) {
        if (itemName.find("Largest Bullet Bag") != std::string_view::npos) {
            return &kBulletBagUpgradeRecipes[1];
        }
        if (itemName.find("Large Bullet Bag") != std::string_view::npos) {
            return &kBulletBagUpgradeRecipes[0];
        }
    }
    if (canonicalId.ends_with("_HOOKSHOT") &&
        itemName.find("Longshot") != std::string_view::npos) {
        return &kLongshotRecipe;
    }
    if (canonicalId.ends_with("_MAGIC_UPGRADE") &&
        itemName.find("Larger Magic Upgrade") != std::string_view::npos) {
        return &kLargeMagicRecipe;
    }
    if (canonicalId.ends_with("_STRENGTH")) {
        if (itemName.find("Golden Gauntlets") != std::string_view::npos) {
            return &kGauntletRecipes[1];
        }
        if (itemName.find("Silver Gauntlets") != std::string_view::npos) {
            return &kGauntletRecipes[0];
        }
    }
    if (canonicalId.ends_with("_WALLET") &&
        (itemName.find("Giant") != std::string_view::npos ||
         itemName.find("Colossal") != std::string_view::npos ||
         itemName.find("Bottomless") != std::string_view::npos)) {
        return &kGiantWalletRecipe;
    }
    if (canonicalId.ends_with("_SWORD") || canonicalId.ends_with("_SWORD_GORON")) {
        const char* resolved = itemName.find("Master Sword") != std::string_view::npos
                                   ? "OOT_SWORD_MASTER"
                               : itemName.find("Razor Sword") != std::string_view::npos
                                   ? "MM_SWORD_RAZOR"
                               : itemName.find("Gilded Sword") != std::string_view::npos
                                   ? "MM_SWORD_GILDED"
                               : itemName.find("Giant's Knife") != std::string_view::npos
                                   ? "OOT_SWORD_KNIFE"
                               : itemName.find("Biggoron's Sword") != std::string_view::npos
                                   ? "OOT_SWORD_BIGGORON"
                                   : nullptr;
        if (resolved != nullptr) {
            return &kRecipes.at(resolved);
        }
    }
    if (canonicalId.ends_with("_SHIELD")) {
        const char* resolved = itemName.find("Deku Shield") != std::string_view::npos
                                   ? "OOT_SHIELD_DEKU"
                               : itemName.find("Hylian Shield") != std::string_view::npos
                                   ? "OOT_SHIELD_HYLIAN"
                               : itemName.find("Hero's Shield") != std::string_view::npos
                                   ? "MM_SHIELD_HERO"
                               : itemName.find("Mirror Shield") != std::string_view::npos
                                   ? "OOT_SHIELD_MIRROR"
                                   : nullptr;
        if (resolved != nullptr) {
            return &kRecipes.at(resolved);
        }
    }
    return nullptr;
}

bool IsMmCanonical(std::string_view suffix) {
    static constexpr std::array<std::string_view, 13> kMmItems = {
        "GREAT_FAIRY_SWORD", "POWDER_KEG", "MASK_BLAST", "MASK_STONE", "MASK_KAMARO",
        "MASK_GORON", "MASK_ZORA", "MASK_KEATON", "MASK_TRUTH", "SWORD_RAZOR",
        "SWORD_GILDED", "BOMBCHU_BAG", "TRANSCENDENT_FAIRY",
    };
    return std::find(kMmItems.begin(), kMmItems.end(), suffix) != kMmItems.end();
}

std::string WithPrefix(std::string_view prefix, std::string_view suffix) {
    std::string result(prefix);
    result.append(suffix);
    return result;
}

} // namespace

std::string OotmmItemRenderCatalog::CanonicalId(std::string_view itemId) {
    if (itemId.starts_with("SHARED_")) {
        const std::string_view suffix = itemId.substr(7);
        const std::string ootId = WithPrefix("OOT_", suffix);
        const std::string mmId = WithPrefix("MM_", suffix);
        if (IsMmCanonical(suffix) || (!kRecipes.contains(ootId) && kRecipes.contains(mmId))) {
            return mmId;
        }
        return ootId;
    }
    if (itemId.starts_with("OOT_")) {
        const std::string_view suffix = itemId.substr(4);
        const std::string mmId = WithPrefix("MM_", suffix);
        if (IsMmCanonical(suffix) || (!kRecipes.contains(itemId) && kRecipes.contains(mmId))) {
            return mmId;
        }
    }
    if (itemId.starts_with("MM_") && !IsMmCanonical(itemId.substr(3))) {
        const std::string ootId = WithPrefix("OOT_", itemId.substr(3));
        if (kRecipes.contains(ootId) && !kRecipes.contains(itemId)) {
            return ootId;
        }
    }
    return std::string(itemId);
}

const OotmmRenderRecipe* OotmmItemRenderCatalog::Find(
    std::string_view itemId, std::string_view itemName) {
    if (itemId.find("_RUSTY_KEY_") != std::string_view::npos) {
        return &kRustyKeyRecipe;
    }
    if (const auto trap = kTrapRecipes.find(itemId); trap != kTrapRecipes.end()) {
        return &trap->second;
    }
    if (itemId == "MM_STRAY_FAIRY_WF") {
        return &kStrayFairyRecipes[0];
    }
    if (itemId == "MM_STRAY_FAIRY_SH") {
        return &kStrayFairyRecipes[1];
    }
    if (itemId == "MM_STRAY_FAIRY_GB") {
        return &kStrayFairyRecipes[2];
    }
    if (itemId == "MM_STRAY_FAIRY_ST") {
        return &kStrayFairyRecipes[3];
    }
    if (itemId == "MM_STRAY_FAIRY_TOWN") {
        return &kStrayFairyRecipes[4];
    }
    if (itemId == "MM_TRANSCENDENT_FAIRY") {
        return &kStrayFairyRecipes[5];
    }
    if (itemId.find("FISHING_POND") != std::string_view::npos) {
        return &kPondFishRecipes[itemId.find("_LOACH_") != std::string_view::npos ? 1 : 0];
    }
    const std::string canonical = CanonicalId(itemId);
    if (const OotmmRenderRecipe* progressive = ProgressiveRecipe(canonical, itemName)) {
        return progressive;
    }
    const auto found = kRecipes.find(canonical);
    return found == kRecipes.end() ? nullptr : &found->second;
}

int32_t OotmmItemRenderCatalog::ResolveParam(
    const OotmmRenderRecipe& recipe, std::string_view itemName) {
    if (recipe.Kind != OotmmRenderKind::SCALE) {
        return recipe.Param;
    }
    if (itemName.find("Bronze Scale") != std::string_view::npos) {
        return 1;
    }
    if (itemName.find("Golden Scale") != std::string_view::npos) {
        return 3;
    }
    return 2;
}

std::string OotmmItemRenderCatalog::ResourcePath(
    const OotmmRenderRecipe& recipe, size_t index, OotmmModelGame targetGame) {
    if (index >= recipe.DisplayLists.size() || recipe.DisplayLists[index].empty()) {
        return {};
    }

    std::string path(recipe.DisplayLists[index]);
    if (recipe.SourceGame != targetGame) {
        const std::string marker = "/object_";
        const size_t object = path.find(marker);
        if (object != std::string::npos) {
            path.replace(object, marker.size(), recipe.SourceGame == OotmmModelGame::Oot ? "/ot_obj_" : "/mm_obj_");
        } else if (const size_t keep = path.find("/gameplay_keep/"); keep != std::string::npos) {
            path.replace(keep, std::string_view("/gameplay_keep").size(),
                         recipe.SourceGame == OotmmModelGame::Oot ? "/ot_gameplay_keep" : "/mm_gameplay_keep");
        }
    }
    return "__OTR__" + path;
}

std::vector<std::string_view> OotmmItemRenderCatalog::AllIds() {
    std::vector<std::string_view> ids;
    ids.reserve(kRecipes.size() + kTrapRecipes.size());
    for (const auto& entry : kRecipes) {
        ids.push_back(entry.first);
    }
    for (const auto& entry : kTrapRecipes) {
        ids.push_back(entry.first);
    }
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
}

const std::vector<OotmmDebugPresentationItem>& OotmmItemRenderCatalog::DebugOnlyItems() {
    static const std::vector<OotmmDebugPresentationItem> kItems = {
        { "OOT_TRAP_ICE", "Ice Trap", "oot" },
        { "OOT_TRAP_FIRE", "Fire Trap", "oot" },
        { "OOT_TRAP_SHOCK", "Shock Trap", "oot" },
        { "OOT_TRAP_DRAIN", "Drain Trap", "oot" },
        { "OOT_TRAP_ANTI_MAGIC", "Anti-Magic Trap", "oot" },
        { "OOT_TRAP_KNOCKBACK", "Knockback Trap", "oot" },
        { "OOT_TRAP_RUPOOR", "Rupoor Trap", "oot" },
    };
    return kItems;
}

} // namespace Ship
