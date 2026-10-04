#pragma once

#include "libultraship/bridge/OotmmGameState.h"

#include <cstdint>
#include <vector>

namespace Ship {

enum class OotmmMenuIcon : uint8_t {
    Key,
    BossKey,
    Map,
    Compass,
    Fairy,
    Skull,
    Triforce,
    Rupee,
    Coin,
    Sun,
    Moon,
};

struct OotmmMenuQuad {
    bool IsIcon = false;
    char Glyph = ' ';
    OotmmMenuIcon Icon = OotmmMenuIcon::Key;
    float X = 0.0f;
    float Y = 0.0f;
    uint32_t Color = 0xFFFFFFFF;
};

struct OotmmMenuTriforce {
    uint32_t Count = 0;
    uint32_t Max = 0;
};

class OotmmInfoMenu {
  public:
    static constexpr int kLines = 9;
    static constexpr float kLeft = -110.0f;
    static constexpr float kTop = 59.0f;
    static constexpr float kWidth = 217.0f;
    static constexpr float kHeight = 128.0f;
    static constexpr float kGlyphWidth = 8.0f;
    static constexpr float kGlyphHeight = 12.0f;
    static constexpr float kIconSize = 12.0f;

    static const char* GlyphTexture(char glyph);
    static const char* IconTexture(OotmmMenuIcon icon);

    bool IsOpen() const;
    int Screen() const;
    int Cursor() const;
    bool Next(const OotmmGameState& state, const OotmmInventory& inventory);
    bool Scroll(const OotmmGameState& state, float stickY);
    std::vector<OotmmMenuQuad> Layout(const OotmmGameState& state, const OotmmInventory& inventory,
                                      const OotmmMenuTriforce& triforce) const;

  private:
    int mScreen = 0;
    int mCursor = 0;
    int mDelay = 0;
};

} // namespace Ship
