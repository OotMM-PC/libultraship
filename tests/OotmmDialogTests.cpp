#include "libultraship/bridge/OotmmDialog.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>

namespace {

using namespace Ship;

void Require(bool value, const std::string& message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

OotmmDialogFont MmFont() {
    static constexpr int kWidths[] = {
        8,  8,  6,  9,  9,  14, 12, 3,  7,  7,  7,  9,  4,  6,  4,  9,  10, 5,  9,  9,  10, 9,  9,  9,
        9,  9,  6,  6,  9,  11, 9,  11, 13, 12, 9,  11, 11, 8,  8,  12, 10, 4,  8,  10, 8,  13, 11, 13,
        9,  13, 10, 10, 9,  10, 11, 15, 11, 10, 10, 7,  10, 7,  10, 9,  5,  8,  9,  8,  9,  9,  6,  9,
        8,  4,  6,  8,  4,  12, 9,  9,  9,  9,  7,  8,  7,  8,  9,  12, 8,  9,  8,  7,  5,  7,  10,
    };
    OotmmDialogFont font;
    for (size_t i = 0; i < font.Advance.size(); ++i) {
        font.Advance[i] = static_cast<int16_t>(kWidths[i] * 0.75f);
    }
    font.Advance[0] = 6;
    return font;
}

OotmmDialogBox MmBox() {
    OotmmDialogBox box;
    box.BodyWidth = 213;
    box.OptionWidth = 203;
    return box;
}

std::string Text(const OotmmDialogLine& line) {
    std::string text;
    for (const auto& c : line.Chars) {
        text.push_back(c.Code);
    }
    return text;
}

std::string Glyphs(const OotmmDialogLayout& layout, int first, int last) {
    std::string glyphs;
    for (int i = first; i <= last; ++i) {
        for (const auto& c : layout.Lines[static_cast<size_t>(i)].Chars) {
            if (c.Code != ' ') {
                glyphs.push_back(c.Code);
            }
        }
    }
    return glyphs;
}

std::string WithoutSpaces(std::string text) {
    text.erase(std::remove(text.begin(), text.end(), ' '), text.end());
    return text;
}

void CheckStructure(const OotmmDialogSpec& spec, const OotmmDialogFont& font, const OotmmDialogBox& box,
                    const OotmmDialogLayout& layout, const std::string& label) {
    const int16_t widest = *std::max_element(font.Advance.begin(), font.Advance.end());
    Require(layout.OptionFirstLine.size() == spec.Options.size(), label + ": option count");
    Require(layout.OptionLastLine.size() == spec.Options.size(), label + ": option count");

    int expected = spec.Options.empty() ? static_cast<int>(layout.Lines.size()) : layout.OptionFirstLine[0];
    for (int i = 0; i < expected; ++i) {
        Require(layout.Lines[static_cast<size_t>(i)].Option == -1, label + ": body line owner");
    }
    for (size_t option = 0; option < spec.Options.size(); ++option) {
        Require(layout.OptionFirstLine[option] == expected, label + ": options are contiguous");
        Require(layout.OptionLastLine[option] >= layout.OptionFirstLine[option], label + ": option has a line");
        for (int i = layout.OptionFirstLine[option]; i <= layout.OptionLastLine[option]; ++i) {
            Require(layout.Lines[static_cast<size_t>(i)].Option == static_cast<int16_t>(option),
                    label + ": option line owner");
        }
        expected = layout.OptionLastLine[option] + 1;
    }
    Require(expected == static_cast<int>(layout.Lines.size()), label + ": every line is owned");

    for (const auto& line : layout.Lines) {
        const int16_t limit = line.Option < 0 ? box.BodyWidth : box.OptionWidth;
        const bool emptyOption = line.Chars.empty() && line.Option >= 0;
        Require(!line.Chars.empty() || emptyOption, label + ": no blank body lines");
        if (line.Chars.empty()) {
            continue;
        }
        Require(line.Chars.front().Code != ' ' && line.Chars.back().Code != ' ', label + ": trimmed line");
        Require(line.Width <= std::max<int16_t>(limit, 1) || line.Chars.size() == 1, label + ": line fits the box");
        Require(line.Indent == 0 || (line.Option >= 0 && limit - line.Indent >= widest), label + ": indent leaves room");
        const auto glyphs = std::count_if(line.Chars.begin(), line.Chars.end(), [](const auto& c) { return c.Code != ' '; });
        Require(glyphs <= std::max<int16_t>(box.MaxLineGlyphs, 1), label + ": glyph budget");
        int16_t x = line.Indent;
        for (const auto& c : line.Chars) {
            Require(c.Code >= OotmmDialogFont::kFirst && c.Code <= OotmmDialogFont::kLast, label + ": drawable glyph");
            Require(c.X == x, label + ": glyph position");
            x = static_cast<int16_t>(x + font.Of(c.Code));
        }
        Require(line.Width == x, label + ": line width");
    }
}

void CheckView(const OotmmDialogLayout& layout, std::mt19937& random, const std::string& label) {
    OotmmDialogView view(layout);
    const int total = static_cast<int>(layout.Lines.size());
    const int maxTop = std::max(0, total - layout.VisibleLines);
    Require(view.Top() == 0, label + ": opens at the top");
    for (int step = 0; step < 40; ++step) {
        if (!layout.OptionFirstLine.empty() && random() % 2 == 0) {
            const int option = static_cast<int>(random() % layout.OptionFirstLine.size());
            view.Reveal(option);
            Require(view.Shows(option), label + ": revealed option is shown");
            Require(view.Row(layout.OptionFirstLine[static_cast<size_t>(option)]) >= 0, label + ": cursor row exists");
        } else {
            view.Scroll(static_cast<int>(random() % 9) - 4);
        }
        Require(view.Top() >= 0 && view.Top() <= maxTop, label + ": scroll stays in range");
        int shown = 0;
        for (int line = 0; line < total; ++line) {
            const int row = view.Row(line);
            Require(row == (line >= view.Top() && line < view.Top() + layout.VisibleLines ? line - view.Top() : -1),
                    label + ": row mapping");
            shown += row >= 0 ? 1 : 0;
        }
        Require(shown == view.ShownLines(), label + ": shown line count");
        const auto thumb = view.Thumb(48);
        Require(view.Scrollable() == (total > layout.VisibleLines), label + ": scrollable flag");
        Require(thumb.Offset >= 0 && thumb.Offset + thumb.Length <= 48, label + ": thumb stays on the track");
        Require(view.Scrollable() == (thumb.Length > 0), label + ": thumb only when scrollable");
    }
}

void TingleExampleWrapsInsideItsOption() {
    OotmmDialogSpec spec;
    spec.Options = { "Sold out - %r20 Rupees",
                     "Bottle of Blue Fire (Player 3) - %r" + OotmmDialogPrice(40), "No thanks" };
    const auto layout = OotmmDialogLayOut(spec, MmFont(), MmBox());
    CheckStructure(spec, MmFont(), MmBox(), layout, "tingle");
    Require(layout.Lines.size() == 4, "tingle: four lines");
    Require(layout.OptionFirstLine == std::vector<int16_t>({ 0, 1, 3 }), "tingle: option rows");
    Require(Text(layout.Lines[1]) == "Bottle of Blue Fire (Player 3) -", "tingle: first half is " + Text(layout.Lines[1]));
    Require(Text(layout.Lines[2]) == "40 Rupees" && layout.Lines[2].Indent == 8,
            "tingle: remainder is " + Text(layout.Lines[2]));
    Require(layout.Lines[2].Chars.front().Color == OotmmDialogColor::Red, "tingle: colour carries over the break");
    Require(layout.Lines[3].Chars.front().Color == OotmmDialogColor::Green, "tingle: options start green");

    OotmmDialogView view(layout);
    Require(!view.Scrollable() && view.Shows(2), "tingle: fits without scrolling");
}

void LongDialogScrollsAndFollowsTheCursor() {
    OotmmDialogSpec spec;
    spec.Body = "Which one will it be?\nThey are both very fine maps.";
    spec.Options = { "Silver Rupee Pouch (Gerudo Training Ground - Slopes) (Player 3) - 200 Rupees",
                     "Soul of the Shooting Gallery/Town Archery Owner (Player 2) - 200 Rupees", "No thanks" };
    const auto layout = OotmmDialogLayOut(spec, MmFont(), MmBox());
    CheckStructure(spec, MmFont(), MmBox(), layout, "long");
    Require(layout.OptionFirstLine[0] == 2, "long: two body lines");
    Require(layout.Lines.size() > 4, "long: needs scrolling");

    OotmmDialogView view(layout);
    Require(view.Scrollable() && view.Top() == 0, "long: opens on the question");
    Require(!view.Shows(2), "long: last option starts hidden");
    Require(view.Reveal(2) && view.Shows(2), "long: moving down follows the cursor");
    Require(view.Top() == static_cast<int>(layout.Lines.size()) - 4, "long: bottom of the list");
    Require(view.Reveal(0) && view.Shows(0), "long: moving back up follows the cursor");
    Require(view.Top() == std::min<int>(layout.OptionFirstLine[0], std::max(0, layout.OptionLastLine[0] - 3)),
            "long: first option brings back as much of the question as fits");
    Require(!view.Scroll(-10) || view.Top() == 0, "long: scroll clamps at the top");
    view.Scroll(100);
    Require(view.Top() == static_cast<int>(layout.Lines.size()) - 4, "long: scroll clamps at the bottom");
}

void MarkupAndUnsupportedCharacters() {
    OotmmDialogSpec spec;
    spec.Body = "100%% sure? %rRed %wplain %zodd\tTab \xE2\x80\x99quoted\xE2\x80\x99 \xC3\xA9\x01\x7F";
    spec.Options = { "", "   " };
    const auto layout = OotmmDialogLayOut(spec, MmFont(), MmBox());
    CheckStructure(spec, MmFont(), MmBox(), layout, "markup");
    const std::string body = Text(layout.Lines[0]) + " " + Text(layout.Lines[1]);
    Require(body == "100% sure? Red plain %zodd Tab 'quoted' ?", "markup: text is " + body);
    Require(layout.Lines[0].Chars[11].Color == OotmmDialogColor::Red, "markup: red applies");
    Require(layout.OptionFirstLine == std::vector<int16_t>({ 2, 3 }), "markup: empty options keep a row");
    Require(layout.Lines[2].Chars.empty() && layout.Lines[3].Chars.empty(), "markup: empty options are empty");
}

void HardBreaksAWordThatCannotFit() {
    OotmmDialogSpec spec;
    spec.Options = { std::string(120, 'W'), "a-b/c" };
    OotmmDialogBox box = MmBox();
    const auto layout = OotmmDialogLayOut(spec, MmFont(), box);
    CheckStructure(spec, MmFont(), box, layout, "hard break");
    Require(Glyphs(layout, layout.OptionFirstLine[0], layout.OptionLastLine[0]) == spec.Options[0], "hard break: keeps text");
    Require(layout.OptionLastLine[0] - layout.OptionFirstLine[0] >= 5, "hard break: spans lines");

    box.OptionWidth = 20;
    const auto narrow = OotmmDialogLayOut(spec, MmFont(), box);
    CheckStructure(spec, MmFont(), box, narrow, "narrow");
    Require(Text(narrow.Lines[static_cast<size_t>(narrow.OptionFirstLine[1])]) == "a-", "narrow: breaks after a hyphen");
}

void ScrollInputRepeatsWhileHeld() {
    OotmmDialogScrollInput input;
    std::string fired;
    for (int frame = 0; frame < 16; ++frame) {
        fired.push_back(input.Step(-80, false, false) != 0 ? 'x' : '.');
    }
    Require(fired == "x.........x..x..", "scroll input: repeat pattern is " + fired);
    Require(input.Step(0, false, false) == 0, "scroll input: idle");
    Require(input.Step(80, false, false) == -1, "scroll input: stick up scrolls up");
    Require(input.Step(80, false, true) == 1, "scroll input: buttons win and restart the repeat");
}

OotmmDialogInput Stick(int choice, int stickY) {
    OotmmDialogInput input;
    input.Choice = choice;
    input.StickY = stickY;
    return input;
}

void LeftStickScrollsUntilTheCursorIsOnScreen() {
    OotmmDialogSpec spec;
    spec.Body = "One\nTwo\nThree\nFour\nFive\nSix";
    spec.Options = { "First", "Second" };
    const auto layout = OotmmDialogLayOut(spec, MmFont(), MmBox());
    Require(layout.OptionFirstLine == std::vector<int16_t>({ 6, 7 }), "controller: answers sit below the box");

    OotmmDialogController controller(layout);
    Require(!controller.View().Shows(0), "controller: first answer starts off-screen");
    for (int press = 1; press <= 3; ++press) {
        const auto moved = controller.Navigate(Stick(1, -80));
        Require(moved.Choice == 0 && moved.Scrolled, "controller: down scrolls instead of skipping an answer");
        Require(controller.View().Top() == press, "controller: one line per press");
        Require(!controller.Navigate(Stick(0, -80)).Scrolled, "controller: a held stick does not repeat");
        controller.Navigate(Stick(0, 0));
    }
    Require(controller.View().Shows(0), "controller: first answer is now on screen");
    Require(controller.Navigate(Stick(1, -80)).Choice == 1, "controller: the cursor moves once it is on screen");
    Require(controller.View().Shows(1), "controller: view follows the cursor");
    controller.Navigate(Stick(1, 0));

    OotmmDialogInput away;
    away.Choice = 1;
    away.ScrollStickY = 80;
    for (int frame = 0; frame < 60; ++frame) {
        controller.Navigate(away);
    }
    Require(controller.View().Top() == 0 && !controller.View().Shows(1), "controller: right stick scrolls away");
    Require(controller.GuardConfirm(), "controller: confirm is held while the cursor is off-screen");
    Require(controller.View().Shows(1) && !controller.GuardConfirm(), "controller: second confirm goes through");

    controller.Navigate(away);
    for (int frame = 0; frame < 60; ++frame) {
        controller.Navigate(away);
    }
    Require(controller.View().Top() == 0, "controller: cannot scroll above the top");
    Require(controller.Navigate(Stick(0, 0)).Choice == 0, "controller: a move away from the hidden answer is kept");
    Require(controller.View().Shows(0), "controller: the view follows that move");
}

void PressingAwayFromAHiddenAnswerMovesTheCursor() {
    OotmmDialogSpec spec;
    spec.Body = "Question";
    spec.Options = { "First", "Second " + std::string(120, 'W'), "Third" };
    const auto layout = OotmmDialogLayOut(spec, MmFont(), MmBox());
    const int last = static_cast<int>(layout.Lines.size()) - 1;
    Require(layout.OptionFirstLine[2] == last && last >= 5, "away: the last answer sits below the box");

    OotmmDialogController controller(layout);
    OotmmDialogInput bottom;
    bottom.ScrollStickY = -80;
    for (int frame = 0; frame < 120; ++frame) {
        controller.Navigate(bottom);
    }
    Require(controller.View().Side(0) == -1, "away: the highlighted answer is above the view");
    Require(controller.View().Row(last) >= 0, "away: the last answer is on screen");

    const auto down = controller.Navigate(Stick(1, -80));
    Require(down.Choice == 1 && controller.View().Row(layout.OptionFirstLine[1]) >= 0,
            "away: down picks the next answer instead of doing nothing");
    controller.Navigate(Stick(1, 0));

    int choice = 1;
    for (int press = 0; press < 20 && choice != 2; ++press) {
        choice = controller.Navigate(Stick(2, -80)).Choice;
        controller.Navigate(Stick(choice, 0));
    }
    Require(choice == 2 && controller.View().Shows(2), "away: the last answer can be reached");

    OotmmDialogInput top;
    top.Choice = 2;
    top.ScrollStickY = 80;
    for (int frame = 0; frame < 120; ++frame) {
        controller.Navigate(top);
    }
    Require(controller.View().Side(2) == 1, "away: the highlighted answer is below the view");
    const auto up = controller.Navigate(Stick(1, 80));
    Require(up.Choice == 1 && controller.View().Row(layout.OptionFirstLine[1]) >= 0,
            "away: up picks the previous answer instead of doing nothing");
}

void EveryAnswerStaysReachable(const OotmmDialogLayout& layout, std::mt19937& random, const std::string& label) {
    const int count = static_cast<int>(layout.OptionFirstLine.size());
    if (count == 0) {
        return;
    }
    OotmmDialogController controller(layout);
    int choice = 0;
    const int budget = static_cast<int>(layout.Lines.size()) * (count + 1) + count;
    for (int leg = 0; leg < 4; ++leg) {
        OotmmDialogInput drift;
        drift.Choice = choice;
        drift.ScrollStickY = random() % 2 == 0 ? 80 : -80;
        for (int frame = static_cast<int>(random() % 80); frame > 0; --frame) {
            choice = controller.Navigate(drift).Choice;
        }
        drift.ScrollStickY = 0;
        controller.Navigate(drift);

        const int direction = leg % 2 == 0 ? 1 : -1;
        const int target = direction > 0 ? count - 1 : 0;
        for (int press = 0; press < budget; ++press) {
            const int before = choice;
            const int side = controller.View().Side(before);
            choice = controller.Navigate(Stick(std::clamp(choice + direction, 0, count - 1), -80 * direction)).Choice;
            Require(std::abs(choice - before) <= 1, label + ": a press never skips an answer");
            Require(choice == before || side != direction,
                    label + ": the cursor only leaves a hidden answer when pressed away from it");
            choice = controller.Navigate(Stick(choice, 0)).Choice;
            if (choice == target && controller.View().Shows(target)) {
                break;
            }
        }
        Require(choice == target, label + ": the cursor reaches the end answer");
        controller.GuardConfirm();
        Require(controller.View().Shows(target), label + ": the end answer can be brought on screen");
    }
}

std::string RandomText(std::mt19937& random, bool anyByte) {
    static const std::string kAlphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -/()':.,!?";
    static const std::string kWide = "WMm@&%il. ";
    std::string text;
    const size_t length = random() % 6 == 0 ? random() % 600 : random() % 90;
    const int style = static_cast<int>(random() % 4);
    for (size_t i = 0; i < length; ++i) {
        if (anyByte && random() % 5 == 0) {
            text.push_back(static_cast<char>(random() % 256));
        } else if (style == 0) {
            text.push_back(kWide[random() % kWide.size()]);
        } else if (style == 1 && random() % 12 != 0) {
            text.push_back(kAlphabet[random() % 52]);
        } else {
            text.push_back(kAlphabet[random() % kAlphabet.size()]);
        }
    }
    return text;
}

void RandomTextAlwaysLaysOut() {
    std::mt19937 random(20261115);
    for (int round = 0; round < 20000; ++round) {
        const bool anyByte = round % 2 == 0;
        OotmmDialogFont font = MmFont();
        if (round % 3 == 0) {
            for (auto& advance : font.Advance) {
                advance = static_cast<int16_t>(1 + random() % 16);
            }
        }
        OotmmDialogBox box;
        const bool realistic = round % 4 != 0;
        box.BodyWidth = static_cast<int16_t>(realistic ? 213 : random() % 320);
        box.OptionWidth = static_cast<int16_t>(realistic ? 203 : random() % 320);
        box.ContinuationIndent = static_cast<int16_t>(realistic ? 8 : random() % 40);
        box.VisibleLines = static_cast<int16_t>(realistic ? 4 : random() % 7);
        box.MaxLineGlyphs = static_cast<int16_t>(realistic ? 48 : random() % 64);

        OotmmDialogSpec spec;
        spec.Body = RandomText(random, anyByte);
        for (size_t i = random() % 4; i > 0; --i) {
            spec.Options.push_back(RandomText(random, anyByte));
        }

        const std::string label = "round " + std::to_string(round);
        const auto layout = OotmmDialogLayOut(spec, font, box);
        CheckStructure(spec, font, box, layout, label);
        if (!anyByte) {
            const int bodyLast = (spec.Options.empty() ? static_cast<int>(layout.Lines.size()) : layout.OptionFirstLine[0]) - 1;
            std::string body = spec.Body;
            body.erase(std::remove(body.begin(), body.end(), '%'), body.end());
            if (spec.Body.find('%') == std::string::npos) {
                Require(Glyphs(layout, 0, bodyLast) == WithoutSpaces(body), label + ": body text survives");
            }
            for (size_t option = 0; option < spec.Options.size(); ++option) {
                if (spec.Options[option].find('%') == std::string::npos) {
                    Require(Glyphs(layout, layout.OptionFirstLine[option], layout.OptionLastLine[option]) ==
                                WithoutSpaces(spec.Options[option]),
                            label + ": option text survives");
                }
            }
        }
        CheckView(layout, random, label);
        EveryAnswerStaysReachable(layout, random, label);

        OotmmDialogController controller(layout);
        int choice = 0;
        for (int frame = 0; frame < 60 && !spec.Options.empty(); ++frame) {
            OotmmDialogInput input;
            input.Choice = std::clamp(choice + static_cast<int>(random() % 3) - 1, 0, static_cast<int>(spec.Options.size()) - 1);
            input.StickY = static_cast<int>(random() % 3) * 80 - 80;
            input.ScrollStickY = static_cast<int>(random() % 3) * 80 - 80;
            choice = controller.Navigate(input).Choice;
            Require(choice >= 0 && choice < static_cast<int>(spec.Options.size()), label + ": choice stays valid");
            if (random() % 7 == 0) {
                controller.GuardConfirm();
                Require(controller.View().Shows(choice) && !controller.GuardConfirm(), label + ": confirm needs one reveal");
            }
        }
    }
}

} // namespace

int main() {
    try {
        TingleExampleWrapsInsideItsOption();
        LongDialogScrollsAndFollowsTheCursor();
        MarkupAndUnsupportedCharacters();
        HardBreaksAWordThatCannotFit();
        ScrollInputRepeatsWhileHeld();
        LeftStickScrollsUntilTheCursorIsOnScreen();
        PressingAwayFromAHiddenAnswerMovesTheCursor();
        RandomTextAlwaysLaysOut();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "ok\n";
    return 0;
}
