#pragma once

#include <cstdint>

namespace Ship {

uintptr_t GetExternalWindowParent();
bool IsWindowEmbedded();
bool IsExternalWindowParentAlive();

} // namespace Ship
