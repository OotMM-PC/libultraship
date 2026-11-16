#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Ship {

struct OotmmPlayerPose {
    uint16_t PlayerId = 0;
    std::string ClientName;
    std::string Game = "oot";
    int SceneId = -1;
    int RoomId = 0;
    int Place = 0;
    float Pos[3] = { 0.0f, 0.0f, 0.0f };
    int16_t Yaw = 0;
    float YOffset = 0.0f;
    int MoveFlags = 0;
    int Form = 0;
    int HumanModel = 0;
    uint32_t Seq = 0;
    std::string DlLeftHand;
    std::string DlRightHand;
    std::string DlSheath;
    std::string DlWaist;
    int LeftHandType = -1;
    int RightHandType = -1;
    int Tunic = 0;
    int Boots = 0;
    int Strength = 0;
    int Mask = 0;
    int ItemAction = -1;
    int CustomMask = 0;
    int Shield = 0;
    int SheathType = 0;
    bool DekuShield = false;
    int CustomLeftHand = 0;
    int CustomRightHand = 0;
    int CustomBoots = 0;
    bool Guard = false;
    std::vector<int16_t> JointTable;
};

enum OotmmPvpKind {
    OOTMM_PVP_FLINCH = 0,
    OOTMM_PVP_STAGGER = 1,
    OOTMM_PVP_KNOCKDOWN = 2,
    OOTMM_PVP_BLAST = 3,
    OOTMM_PVP_FIRE = 4,
    OOTMM_PVP_ICE = 5,
    OOTMM_PVP_SHOCK = 6,
    OOTMM_PVP_BLOCKED = 7,
    OOTMM_PVP_STUN = 8,
};

struct OotmmPvpHit {
    uint16_t SourcePlayer = 0;
    uint16_t TargetPlayer = 0;
    int Damage = 0;
    int16_t Yaw = 0;
    uint32_t Seq = 0;
    int Kind = OOTMM_PVP_KNOCKDOWN;
};

class OotmmPresenceIo {
  public:
    static std::string SerializePose(const OotmmPlayerPose& pose);
    static std::string SerializePvpHit(const OotmmPvpHit& hit);
    static bool ParseRoster(const std::string& json, std::vector<OotmmPlayerPose>& poses);
    static std::optional<OotmmPvpHit> ParsePvpHit(const std::string& json);
};

} // namespace Ship
