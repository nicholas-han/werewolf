#include <gtest/gtest.h>

#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "core/board.h"
#include "core/enums.h"
#include "core/game_state.h"
#include "flow/game.h"
#include "flow/transcript.h"
#include "flow/win_condition.h"
#include "io/console_decision_provider.h"
#include "io/scripted_decision_provider.h"

using namespace ww;

namespace {

// Tiny deterministic board: seat1 = wolf, seat2/3 = civilians. KillAll so a
// godless probe isn't a vacuous 屠边 win; no sheriff so the day flow is minimal.
Board speechProbe() {
    Board b;
    b.name = "speech-probe";
    b.roster = {{RoleKind::Werewolf, 1}, {RoleKind::Civilian, 2}};
    b.config.winRule = WinRule::KillAll;
    b.config.sheriffEnabled = false;
    return b;
}

bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

// One-day script: peaceful night, all three speak, then seat1 (wolf) is exiled
// -> TownWins. `speeches` are fed FIFO (3 statements + 1 last words).
void scriptOneDay(ScriptedDecisionProvider& provider) {
    provider.nightKills = {std::nullopt};  // night 1: 空刀 -> peaceful
    provider.votes = {1, 1, 1};            // day 1: everyone exiles the wolf (seat1)
}

}  // namespace

TEST(SpeechLog, DaySpeechesAndLastWordsRecorded) {
    ScriptedDecisionProvider provider;
    scriptOneDay(provider);
    provider.speeches = {"A", "B", "C", "遗言内容"};  // 3 statements + exiled wolf's 遗言

    Game game(speechProbe(), provider);
    EXPECT_EQ(game.run(), GameResult::TownWins);
    const GameState& st = game.state();

    std::set<int> stmtSeats;
    std::set<std::string> stmtTexts;
    int lastWordsCount = 0;
    for (const auto& e : st.speeches) {
        EXPECT_EQ(e.day, 1);
        if (e.kind == SpeechKind::Statement) {
            stmtSeats.insert(e.seat);
            stmtTexts.insert(e.text);
            EXPECT_EQ(e.seat, st.find(e.speakerId)->seat());
        } else {
            ++lastWordsCount;
            EXPECT_EQ(e.seat, 1);              // only the exiled wolf gets last words
            EXPECT_EQ(e.text, "遗言内容");
        }
    }
    // Order is time-based, so assert as sets: every alive seat spoke exactly once.
    EXPECT_EQ(stmtSeats, (std::set<int>{1, 2, 3}));
    EXPECT_EQ(stmtTexts, (std::set<std::string>{"A", "B", "C"}));
    EXPECT_EQ(lastWordsCount, 1);
    EXPECT_EQ(st.speeches.size(), 4u);
}

TEST(SpeechLog, EmptySpeechesArePassedNotRecorded) {
    // No speeches queued -> collectSpeech returns "" -> nothing stored, game runs.
    ScriptedDecisionProvider provider;
    scriptOneDay(provider);
    Game game(speechProbe(), provider);
    EXPECT_EQ(game.run(), GameResult::TownWins);
    EXPECT_TRUE(game.state().speeches.empty());
}

TEST(SpeechLog, TranscriptRendersDayAndEntries) {
    ScriptedDecisionProvider provider;
    scriptOneDay(provider);
    provider.speeches = {"我是好人", "查杀1号", "我跟随预言家", "认输了"};
    Game game(speechProbe(), provider);
    game.run();

    const std::string t = formatTranscript(game.state());
    EXPECT_TRUE(contains(t, "复盘：发言记录"));
    EXPECT_TRUE(contains(t, "第 1 天"));
    EXPECT_TRUE(contains(t, "我是好人"));
    EXPECT_TRUE(contains(t, "认输了"));
    EXPECT_TRUE(contains(t, "（发言）"));
    EXPECT_TRUE(contains(t, "（遗言）"));
}

TEST(SpeechLog, TranscriptEmptyWhenNothingRecorded) {
    GameState st = buildInitialState(speechProbe());
    EXPECT_TRUE(contains(formatTranscript(st), "本局无发言记录"));
}

TEST(SpeechLog, ConsolePromptsOnlyWhenRecordingEnabled) {
    GameState s = buildInitialState(makeBoard9_SeerWitchHunter());

    // Recording on: the typed line is captured.
    std::istringstream in("我怀疑 3 号\n");
    std::ostringstream out;
    ConsoleDecisionProvider on(in, out);
    on.setRecordSpeech(true);
    EXPECT_EQ(on.collectSpeech(s, 3, SpeechKind::Statement, 1), "我怀疑 3 号");
    EXPECT_TRUE(contains(out.str(), "记录发言"));

    // Recording off (default): returns "" and does NOT consume the input line.
    std::istringstream in2("不应被读取\n");
    std::ostringstream out2;
    ConsoleDecisionProvider off(in2, out2);
    EXPECT_EQ(off.collectSpeech(s, 3, SpeechKind::Statement, 1), "");
    EXPECT_TRUE(out2.str().empty());
    std::string leftover;
    std::getline(in2, leftover);
    EXPECT_EQ(leftover, "不应被读取");  // input untouched
}
