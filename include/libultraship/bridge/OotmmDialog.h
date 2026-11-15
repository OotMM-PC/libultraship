#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Ship {

enum class OotmmDialogColor : uint8_t {
    Default,
    Red,
    Green,
    Blue,
    LightBlue,
    Pink,
    Yellow,
    Orange,
};

struct OotmmDialogFont {
    static constexpr char kFirst = ' ';
    static constexpr char kLast = '~';

    std::array<int16_t, kLast - kFirst + 1> Advance{};

    int16_t Of(char code) const {
        return Advance[static_cast<size_t>(code - kFirst)];
    }
};

struct OotmmDialogBox {
    int16_t BodyWidth = 0;
    int16_t OptionWidth = 0;
    int16_t ContinuationIndent = 8;
    int16_t VisibleLines = 4;
    int16_t MaxLineGlyphs = 48;
};

struct OotmmDialogSpec {
    std::string Body;
    std::vector<std::string> Options;
    OotmmDialogColor OptionColor = OotmmDialogColor::Green;
};

struct OotmmDialogChar {
    char Code = ' ';
    OotmmDialogColor Color = OotmmDialogColor::Default;
    int16_t X = 0;
};

struct OotmmDialogLine {
    std::vector<OotmmDialogChar> Chars;
    int16_t Option = -1;
    int16_t Indent = 0;
    int16_t Width = 0;
};

struct OotmmDialogLayout {
    std::vector<OotmmDialogLine> Lines;
    std::vector<int16_t> OptionFirstLine;
    std::vector<int16_t> OptionLastLine;
    int16_t VisibleLines = 4;
};

inline constexpr char OotmmDialogNoBreakSpace[] = "\xC2\xA0";

/// Wraps the body and every option to the box. Text is ASCII with %w %r %g %b %c %p %y %o colour marks; a body
/// newline starts a new line and anything the fonts cannot draw becomes '?'.
OotmmDialogLayout OotmmDialogLayOut(const OotmmDialogSpec& spec, const OotmmDialogFont& font,
                                    const OotmmDialogBox& box);

/// A rupee amount that never wraps between the number and the word.
std::string OotmmDialogPrice(int rupees);

struct OotmmDialogThumb {
    int16_t Offset = 0;
    int16_t Length = 0;
};

class OotmmDialogView {
  public:
    OotmmDialogView() = default;
    explicit OotmmDialogView(const OotmmDialogLayout& layout);

    int Top() const {
        return mTop;
    }
    int ShownLines() const;
    bool Scrollable() const;
    int Row(int line) const;
    bool Shows(int option) const;
    /// -1 when the option starts above the view, 1 when it runs past the bottom, 0 when it is shown.
    int Side(int option) const;
    bool Scroll(int lines);
    bool Reveal(int option);
    OotmmDialogThumb Thumb(int trackLength) const;

  private:
    int MaxTop() const;

    std::vector<int16_t> mFirst;
    std::vector<int16_t> mLast;
    int mTotal = 0;
    int mVisible = 4;
    int mTop = 0;
};

class OotmmDialogScrollInput {
  public:
    int Step(int stickY, bool up, bool down);

  private:
    int mDirection = 0;
    int mHeld = 0;
};

struct OotmmDialogInput {
    int Choice = 0;
    int StickY = 0;
    int ScrollStickY = 0;
    bool ScrollUp = false;
    bool ScrollDown = false;
};

struct OotmmDialogOutcome {
    int Choice = 0;
    bool Scrolled = false;
};

class OotmmDialogController {
  public:
    OotmmDialogController() = default;
    explicit OotmmDialogController(const OotmmDialogLayout& layout);

    const OotmmDialogView& View() const {
        return mView;
    }

    /// Takes the choice the game moved to this frame and returns the choice it should keep.
    OotmmDialogOutcome Navigate(const OotmmDialogInput& input);

    /// True when a confirm press must be swallowed because the highlighted option was off-screen.
    bool GuardConfirm();

  private:
    OotmmDialogView mView;
    OotmmDialogScrollInput mScroll;
    int mChoice = 0;
    bool mStickHeld = false;
};

} // namespace Ship
