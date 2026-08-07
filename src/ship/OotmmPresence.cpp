#include "libultraship/bridge/OotmmPresence.h"

#include <nlohmann/json.hpp>

#include <sstream>

namespace Ship {

namespace {

std::string Escape(const std::string& value) {
    return nlohmann::json(value).dump();
}

int ReadInt(const nlohmann::json& root, const char* key, int fallback) {
    const auto it = root.find(key);
    return it != root.end() && it->is_number() ? it->get<int>() : fallback;
}

double ReadNumber(const nlohmann::json& root, const char* key, double fallback) {
    const auto it = root.find(key);
    return it != root.end() && it->is_number() ? it->get<double>() : fallback;
}

std::string ReadString(const nlohmann::json& root, const char* key) {
    const auto it = root.find(key);
    return it != root.end() && it->is_string() ? it->get<std::string>() : std::string();
}

std::optional<OotmmPlayerPose> ParsePoseObject(const nlohmann::json& root) {
    if (!root.is_object()) {
        return std::nullopt;
    }

    OotmmPlayerPose pose;
    pose.PlayerId = static_cast<uint16_t>(ReadInt(root, "sourcePlayer", 0));
    pose.ClientName = ReadString(root, "clientName");
    pose.Game = ReadString(root, "game");
    if (pose.Game.empty()) {
        return std::nullopt;
    }
    pose.SceneId = ReadInt(root, "sceneId", -1);
    pose.RoomId = ReadInt(root, "roomId", 0);
    if (const auto pos = root.find("pos"); pos != root.end() && pos->is_array() && pos->size() >= 3) {
        for (int i = 0; i < 3; ++i) {
            pose.Pos[i] = (*pos)[i].is_number() ? (*pos)[i].get<float>() : 0.0f;
        }
    }
    pose.Yaw = static_cast<int16_t>(ReadInt(root, "yaw", 0));
    pose.YOffset = static_cast<float>(ReadNumber(root, "yOff", 0.0));
    pose.MoveFlags = ReadInt(root, "mvf", 0);
    pose.Form = ReadInt(root, "form", 0);
    pose.HumanModel = ReadInt(root, "hm", 0);
    pose.Seq = static_cast<uint32_t>(ReadNumber(root, "seq", 0));
    pose.DlLeftHand = ReadString(root, "dlL");
    pose.DlRightHand = ReadString(root, "dlR");
    pose.DlSheath = ReadString(root, "dlS");
    pose.DlWaist = ReadString(root, "dlW");
    pose.LeftHandType = ReadInt(root, "lht", -1);
    pose.RightHandType = ReadInt(root, "rht", -1);
    pose.Tunic = ReadInt(root, "tun", 0);
    pose.Boots = ReadInt(root, "bts", 0);
    pose.Strength = ReadInt(root, "str", 0);
    pose.Mask = ReadInt(root, "msk", 0);
    pose.ItemAction = ReadInt(root, "ia", -1);
    pose.CustomMask = ReadInt(root, "cmsk", 0);
    pose.Shield = ReadInt(root, "shd", 0);
    pose.SheathType = ReadInt(root, "sth", 0);
    pose.DekuShield = ReadInt(root, "dshd", 0) != 0;
    pose.CustomLeftHand = ReadInt(root, "clh", 0);
    pose.CustomRightHand = ReadInt(root, "crh", 0);
    pose.CustomBoots = ReadInt(root, "cbts", 0);
    if (const auto joints = root.find("joints"); joints != root.end() && joints->is_array()) {
        pose.JointTable.reserve(joints->size());
        for (const auto& j : *joints) {
            pose.JointTable.push_back(static_cast<int16_t>(j.is_number() ? j.get<int>() : 0));
        }
    }
    return pose;
}

} // namespace

std::string OotmmPresenceIo::SerializePose(const OotmmPlayerPose& pose) {
    std::ostringstream out;
    out << "{\"sourcePlayer\":" << pose.PlayerId;
    if (!pose.ClientName.empty()) {
        out << ",\"clientName\":" << Escape(pose.ClientName);
    }
    out << ",\"game\":" << Escape(pose.Game);
    out << ",\"sceneId\":" << pose.SceneId;
    out << ",\"roomId\":" << pose.RoomId;
    out << ",\"pos\":[" << pose.Pos[0] << "," << pose.Pos[1] << "," << pose.Pos[2] << "]";
    out << ",\"yaw\":" << pose.Yaw;
    out << ",\"yOff\":" << pose.YOffset;
    out << ",\"mvf\":" << pose.MoveFlags;
    out << ",\"form\":" << pose.Form;
    if (pose.HumanModel != 0) out << ",\"hm\":" << pose.HumanModel;
    out << ",\"seq\":" << pose.Seq;
    if (!pose.DlLeftHand.empty()) out << ",\"dlL\":" << Escape(pose.DlLeftHand);
    if (!pose.DlRightHand.empty()) out << ",\"dlR\":" << Escape(pose.DlRightHand);
    if (!pose.DlSheath.empty()) out << ",\"dlS\":" << Escape(pose.DlSheath);
    if (!pose.DlWaist.empty()) out << ",\"dlW\":" << Escape(pose.DlWaist);
    if (pose.LeftHandType >= 0) out << ",\"lht\":" << pose.LeftHandType;
    if (pose.RightHandType >= 0) out << ",\"rht\":" << pose.RightHandType;
    if (pose.Tunic != 0) out << ",\"tun\":" << pose.Tunic;
    if (pose.Boots != 0) out << ",\"bts\":" << pose.Boots;
    if (pose.Strength != 0) out << ",\"str\":" << pose.Strength;
    if (pose.Mask != 0) out << ",\"msk\":" << pose.Mask;
    if (pose.ItemAction >= 0) out << ",\"ia\":" << pose.ItemAction;
    if (pose.CustomMask != 0) out << ",\"cmsk\":" << pose.CustomMask;
    if (pose.Shield != 0) out << ",\"shd\":" << pose.Shield;
    if (pose.SheathType != 0) out << ",\"sth\":" << pose.SheathType;
    if (pose.DekuShield) out << ",\"dshd\":1";
    if (pose.CustomLeftHand != 0) out << ",\"clh\":" << pose.CustomLeftHand;
    if (pose.CustomRightHand != 0) out << ",\"crh\":" << pose.CustomRightHand;
    if (pose.CustomBoots != 0) out << ",\"cbts\":" << pose.CustomBoots;
    out << ",\"joints\":[";
    for (size_t i = 0; i < pose.JointTable.size(); ++i) {
        if (i > 0) {
            out << ",";
        }
        out << pose.JointTable[i];
    }
    out << "]}";
    return out.str();
}

std::string OotmmPresenceIo::SerializePvpHit(const OotmmPvpHit& hit) {
    std::ostringstream out;
    out << "{\"sourcePlayer\":" << hit.SourcePlayer;
    out << ",\"targetPlayer\":" << hit.TargetPlayer;
    out << ",\"damage\":" << hit.Damage;
    out << ",\"yaw\":" << hit.Yaw;
    out << ",\"seq\":" << hit.Seq;
    out << "}";
    return out.str();
}

bool OotmmPresenceIo::ParseRoster(const std::string& json, std::vector<OotmmPlayerPose>& poses) {
    const nlohmann::json root = nlohmann::json::parse(json, nullptr, false);
    if (!root.is_object()) {
        return false;
    }

    poses.clear();
    const auto array = root.find("poses");
    if (array == root.end() || !array->is_array()) {
        return true;
    }

    poses.reserve(array->size());
    for (const auto& entry : *array) {
        if (auto pose = ParsePoseObject(entry); pose.has_value()) {
            poses.push_back(std::move(*pose));
        }
    }
    return true;
}

std::optional<OotmmPvpHit> OotmmPresenceIo::ParsePvpHit(const std::string& json) {
    const nlohmann::json root = nlohmann::json::parse(json, nullptr, false);
    if (!root.is_object()) {
        return std::nullopt;
    }

    OotmmPvpHit hit;
    hit.SourcePlayer = static_cast<uint16_t>(ReadInt(root, "sourcePlayer", 0));
    hit.TargetPlayer = static_cast<uint16_t>(ReadInt(root, "targetPlayer", 0));
    hit.Damage = ReadInt(root, "damage", 0);
    hit.Yaw = static_cast<int16_t>(ReadInt(root, "yaw", 0));
    hit.Seq = static_cast<uint32_t>(ReadNumber(root, "seq", 0));
    return hit;
}

} // namespace Ship
