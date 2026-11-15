#include "libultraship/bridge/OotmmDialog.h"

#include <algorithm>
#include <string_view>

namespace Ship {
namespace {

constexpr size_t kMaxSourceBytes = 4096;
constexpr int kStickThreshold = 40;
constexpr int kCursorThreshold = 30;
constexpr int kRepeatDelay = 8;
constexpr int kRepeatInterval = 3;

struct Token {
    char Code = ' ';
    OotmmDialogColor Color = OotmmDialogColor::Default;
    bool LineBreak = false;
    bool Glue = false;
};

bool ColorOf(char mark, OotmmDialogColor& color) {
    switch (mark) {
        case 'w':
            color = OotmmDialogColor::Default;
            return true;
        case 'r':
            color = OotmmDialogColor::Red;
            return true;
        case 'g':
            color = OotmmDialogColor::Green;
            return true;
        case 'b':
            color = OotmmDialogColor::Blue;
            return true;
        case 'c':
            color = OotmmDialogColor::LightBlue;
            return true;
        case 'p':
            color = OotmmDialogColor::Pink;
            return true;
        case 'y':
            color = OotmmDialogColor::Yellow;
            return true;
        case 'o':
            color = OotmmDialogColor::Orange;
            return true;
        default:
            return false;
    }
}

std::string_view Transliterate(std::string_view text, size_t& index) {
    const auto lead = static_cast<uint8_t>(text[index]);
    size_t length = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
    length = std::min(length, text.size() - index);
    const std::string_view sequence = text.substr(index, length);
    index += length - 1;
    if (lead < 0xC0) {
        return "";
    }
    if (sequence == OotmmDialogNoBreakSpace) {
        return " ";
    }
    if (sequence == "\xE2\x80\x98" || sequence == "\xE2\x80\x99") {
        return "'";
    }
    if (sequence == "\xE2\x80\x9C" || sequence == "\xE2\x80\x9D") {
        return "\"";
    }
    if (sequence == "\xE2\x80\x93" || sequence == "\xE2\x80\x94") {
        return "-";
    }
    if (sequence == "\xE2\x80\xA6") {
        return "...";
    }
    return "?";
}

std::vector<Token> Tokenize(std::string_view text, OotmmDialogColor color, bool keepLineBreaks) {
    text = text.substr(0, kMaxSourceBytes);
    std::vector<Token> tokens;
    tokens.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '%' && i + 1 < text.size()) {
            if (text[i + 1] == '%') {
                ++i;
            } else if (ColorOf(text[i + 1], color)) {
                ++i;
                continue;
            }
            tokens.push_back({ '%', color, false });
        } else if (c == '\n') {
            tokens.push_back({ ' ', color, keepLineBreaks });
        } else if (c == '\t' || c == '\r') {
            tokens.push_back({ ' ', color, false });
        } else if (static_cast<uint8_t>(c) >= 0x80) {
            const std::string_view sequence = text.substr(i);
            const bool glue = sequence.starts_with(OotmmDialogNoBreakSpace);
            for (const char replacement : Transliterate(text, i)) {
                tokens.push_back({ replacement, color, false, glue });
            }
        } else if (c >= OotmmDialogFont::kFirst && c <= OotmmDialogFont::kLast) {
            tokens.push_back({ c, color, false });
        }
    }
    return tokens;
}

class Wrapper {
  public:
    Wrapper(const OotmmDialogFont& font, int16_t width, int16_t continuationIndent, int16_t maxGlyphs, int16_t option,
            std::vector<OotmmDialogLine>& lines)
        : mFont(font), mWidth(std::max<int16_t>(width, 1)), mMaxGlyphs(std::max<int16_t>(maxGlyphs, 1)),
          mOption(option), mLines(lines) {
        const int16_t widest = *std::max_element(font.Advance.begin(), font.Advance.end());
        mContinuationIndent = continuationIndent > 0 && mWidth - continuationIndent >= widest ? continuationIndent : 0;
    }

    void Add(const std::vector<Token>& tokens) {
        for (const Token& token : tokens) {
            if (token.LineBreak) {
                Flush();
                mIndent = 0;
            } else {
                Place(token);
            }
        }
        Flush();
    }

    bool Empty() const {
        return mEmitted == 0;
    }

    void EmitEmpty() {
        Emit({});
    }

  private:
    static bool BreaksAfter(char code) {
        return code == '-' || code == '/';
    }

    int Glyphs() const {
        return static_cast<int>(std::count_if(mLine.begin(), mLine.end(), [](const Token& t) { return t.Code != ' '; }));
    }

    int Measure(const std::vector<Token>& tokens) const {
        int width = 0;
        for (const Token& token : tokens) {
            width += mFont.Of(token.Code);
        }
        return width;
    }

    void Place(const Token& token) {
        if (token.Code == ' ' && mLine.empty()) {
            return;
        }
        mLine.push_back(token);
        const bool tooWide = mIndent + Measure(mLine) > mWidth && token.Code != ' ';
        const bool tooMany = Glyphs() > mMaxGlyphs;
        if ((!tooWide && !tooMany) || mLine.size() == 1) {
            return;
        }

        size_t split = mLine.size() - 1;
        for (size_t i = mLine.size() - 1; i > 0; --i) {
            if ((mLine[i - 1].Code == ' ' && !mLine[i - 1].Glue) || BreaksAfter(mLine[i - 1].Code)) {
                split = i;
                break;
            }
        }
        std::vector<Token> rest(mLine.begin() + static_cast<std::ptrdiff_t>(split), mLine.end());
        mLine.resize(split);
        Flush();
        mIndent = mContinuationIndent;
        for (const Token& carried : rest) {
            Place(carried);
        }
    }

    void Flush() {
        while (!mLine.empty() && mLine.back().Code == ' ') {
            mLine.pop_back();
        }
        if (!mLine.empty()) {
            Emit(mLine);
        }
        mLine.clear();
    }

    void Emit(const std::vector<Token>& tokens) {
        OotmmDialogLine line;
        line.Option = mOption;
        line.Indent = mIndent;
        int16_t x = mIndent;
        line.Chars.reserve(tokens.size());
        for (const Token& token : tokens) {
            line.Chars.push_back({ token.Code, token.Color, x });
            x = static_cast<int16_t>(x + mFont.Of(token.Code));
        }
        line.Width = x;
        mLines.push_back(std::move(line));
        ++mEmitted;
    }

    const OotmmDialogFont& mFont;
    int16_t mWidth;
    int16_t mMaxGlyphs;
    int16_t mContinuationIndent = 0;
    int16_t mOption;
    std::vector<OotmmDialogLine>& mLines;
    std::vector<Token> mLine;
    int16_t mIndent = 0;
    int mEmitted = 0;
};

} // namespace

OotmmDialogLayout OotmmDialogLayOut(const OotmmDialogSpec& spec, const OotmmDialogFont& font,
                                    const OotmmDialogBox& box) {
    OotmmDialogLayout layout;
    layout.VisibleLines = std::max<int16_t>(box.VisibleLines, 1);

    Wrapper body(font, box.BodyWidth, 0, box.MaxLineGlyphs, -1, layout.Lines);
    body.Add(Tokenize(spec.Body, OotmmDialogColor::Default, true));

    for (size_t option = 0; option < spec.Options.size(); ++option) {
        layout.OptionFirstLine.push_back(static_cast<int16_t>(layout.Lines.size()));
        Wrapper wrapper(font, box.OptionWidth, box.ContinuationIndent, box.MaxLineGlyphs, static_cast<int16_t>(option),
                        layout.Lines);
        wrapper.Add(Tokenize(spec.Options[option], spec.OptionColor, false));
        if (wrapper.Empty()) {
            wrapper.EmitEmpty();
        }
        layout.OptionLastLine.push_back(static_cast<int16_t>(layout.Lines.size() - 1));
    }
    return layout;
}

std::string OotmmDialogPrice(int rupees) {
    return std::to_string(rupees) + OotmmDialogNoBreakSpace + (rupees == 1 ? "Rupee" : "Rupees");
}

OotmmDialogView::OotmmDialogView(const OotmmDialogLayout& layout)
    : mFirst(layout.OptionFirstLine), mLast(layout.OptionLastLine), mTotal(static_cast<int>(layout.Lines.size())),
      mVisible(std::max<int>(layout.VisibleLines, 1)) {
}

int OotmmDialogView::MaxTop() const {
    return std::max(0, mTotal - mVisible);
}

int OotmmDialogView::ShownLines() const {
    return std::min(mTotal, mVisible);
}

bool OotmmDialogView::Scrollable() const {
    return mTotal > mVisible;
}

int OotmmDialogView::Row(int line) const {
    return line >= mTop && line < mTop + mVisible && line < mTotal ? line - mTop : -1;
}

bool OotmmDialogView::Shows(int option) const {
    if (option < 0 || option >= static_cast<int>(mFirst.size())) {
        return true;
    }
    const int first = mFirst[static_cast<size_t>(option)];
    const int last = std::min<int>(mLast[static_cast<size_t>(option)], first + mVisible - 1);
    return first >= mTop && last < mTop + mVisible;
}

int OotmmDialogView::Side(int option) const {
    if (Shows(option)) {
        return 0;
    }
    return mFirst[static_cast<size_t>(option)] < mTop ? -1 : 1;
}

bool OotmmDialogView::Scroll(int lines) {
    const int top = std::clamp(mTop + lines, 0, MaxTop());
    const bool moved = top != mTop;
    mTop = top;
    return moved;
}

bool OotmmDialogView::Reveal(int option) {
    if (option < 0 || option >= static_cast<int>(mFirst.size())) {
        return false;
    }
    const int first = mFirst[static_cast<size_t>(option)];
    const int last = mLast[static_cast<size_t>(option)];
    int top = mTop;
    if (option == 0) {
        top = std::min(first, std::max(0, last - mVisible + 1));
    } else if (last - first + 1 >= mVisible || first < mTop) {
        top = first;
    } else if (last >= mTop + mVisible) {
        top = last - mVisible + 1;
    }
    top = std::clamp(top, 0, MaxTop());
    const bool moved = top != mTop;
    mTop = top;
    return moved;
}

OotmmDialogThumb OotmmDialogView::Thumb(int trackLength) const {
    OotmmDialogThumb thumb;
    if (!Scrollable() || trackLength <= 0) {
        return thumb;
    }
    thumb.Length = static_cast<int16_t>(std::clamp(trackLength * mVisible / mTotal, std::min(4, trackLength), trackLength));
    thumb.Offset = static_cast<int16_t>((trackLength - thumb.Length) * mTop / MaxTop());
    return thumb;
}

int OotmmDialogScrollInput::Step(int stickY, bool up, bool down) {
    const int direction = up ? -1 : down ? 1 : stickY >= kStickThreshold ? -1 : stickY <= -kStickThreshold ? 1 : 0;
    if (direction != mDirection) {
        mDirection = direction;
        mHeld = 0;
    }
    if (direction == 0) {
        return 0;
    }
    ++mHeld;
    const bool fires = mHeld == 1 || (mHeld > kRepeatDelay && (mHeld - kRepeatDelay) % kRepeatInterval == 0);
    return fires ? direction : 0;
}

OotmmDialogController::OotmmDialogController(const OotmmDialogLayout& layout) : mView(layout) {
}

OotmmDialogOutcome OotmmDialogController::Navigate(const OotmmDialogInput& input) {
    int direction = 0;
    if (input.StickY > -kCursorThreshold && input.StickY < kCursorThreshold) {
        mStickHeld = false;
    } else if (!mStickHeld) {
        mStickHeld = true;
        direction = input.StickY > 0 ? -1 : 1;
    }

    const bool moved = input.Choice != mChoice;
    if (moved) {
        direction = input.Choice > mChoice ? 1 : -1;
    }

    OotmmDialogOutcome outcome;
    const int side = mView.Side(mChoice);
    if (moved && direction != side) {
        mChoice = input.Choice;
        mView.Reveal(mChoice);
    } else if (side != 0 && direction != 0) {
        outcome.Scrolled = mView.Scroll(direction);
    }

    const int step = mScroll.Step(input.ScrollStickY, input.ScrollUp, input.ScrollDown);
    if (step != 0 && mView.Scroll(step)) {
        outcome.Scrolled = true;
    }
    outcome.Choice = mChoice;
    return outcome;
}

bool OotmmDialogController::GuardConfirm() {
    if (mView.Shows(mChoice)) {
        return false;
    }
    mView.Reveal(mChoice);
    return true;
}

} // namespace Ship
