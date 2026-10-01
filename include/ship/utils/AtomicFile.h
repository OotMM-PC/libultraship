#pragma once

#include <filesystem>
#include <string_view>
#include <system_error>

namespace Ship {
bool WriteFileAtomically(const std::filesystem::path& destination, std::string_view contents,
                         std::error_code& error);
}
