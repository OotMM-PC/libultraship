#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <set>
#include <string>

namespace Ship {
struct OotmmRecoveryRequest {
    std::string Backup;
    std::optional<std::string> Primary;

    static std::optional<OotmmRecoveryRequest> Parse(const std::string& text) {
        try {
            bool unique = true;
            std::set<std::string> keys;
            const auto root = nlohmann::json::parse(text, [&](int, nlohmann::json::parse_event_t event,
                                                            nlohmann::json& value) {
                if (event == nlohmann::json::parse_event_t::key && !keys.insert(value.get<std::string>()).second) {
                    unique = false;
                }
                return true;
            });
            if (!unique || !root.is_object() || root.size() != 3 || !root.at("version").is_number_integer() ||
                root.at("version") != 1 || !root.at("backup").is_string() ||
                !(root.at("primaryHex").is_null() || root.at("primaryHex").is_string())) {
                return std::nullopt;
            }
            OotmmRecoveryRequest request;
            request.Backup = root.at("backup").get<std::string>();
            if (request.Backup.empty() || request.Backup.size() > 8 * 1024 * 1024) {
                return std::nullopt;
            }
            if (!root.at("primaryHex").is_null()) {
                const auto hex = root.at("primaryHex").get<std::string>();
                if (hex.size() > 16 * 1024 * 1024 || hex.size() % 2 != 0) {
                    return std::nullopt;
                }
                auto digit = [](char c) -> int {
                    if (c >= '0' && c <= '9') return c - '0';
                    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                    return -1;
                };
                std::string bytes;
                bytes.reserve(hex.size() / 2);
                for (size_t i = 0; i < hex.size(); i += 2) {
                    const int high = digit(hex[i]);
                    const int low = digit(hex[i + 1]);
                    if (high < 0 || low < 0) return std::nullopt;
                    bytes.push_back(static_cast<char>((high << 4) | low));
                }
                request.Primary = std::move(bytes);
            }
            return request;
        } catch (const nlohmann::json::exception&) {
            return std::nullopt;
        }
    }
};
}
