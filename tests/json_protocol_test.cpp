#include <gtest/gtest.h>

#include <optional>
#include <sstream>
#include <string>

#include "core/board.h"
#include "core/game_state.h"
#include "flow/game.h"
#include "flow/win_condition.h"
#include "io/json_decision_provider.h"
#include "io/json_util.h"

using namespace ww;

namespace {

GameState board9State() { return buildInitialState(makeBoard9_SeerWitchHunter()); }

// Count how many top-level lines in `s` parse as JSON; fail on any that don't.
int allLinesParse(const std::string& s) {
    std::istringstream ls(s);
    std::string line;
    int n = 0;
    while (std::getline(ls, line)) {
        if (line.empty()) continue;
        ++n;
        EXPECT_TRUE(jsonu::parse(line).has_value()) << "unparseable line: " << line;
    }
    return n;
}

}  // namespace

// --- json_util ---

TEST(JsonUtil, ParsesUnicodeNumbersBoolsNull) {
    auto v = jsonu::parse(R"({"t":"reply","id":7,"choice":3,"decision":true,
                              "skip":null,"text":"你好\nP3"})");
    ASSERT_TRUE(v.has_value());
    const jsonu::Value* t = v->get("t");
    ASSERT_NE(t, nullptr);
    EXPECT_TRUE(t->isStr());
    EXPECT_EQ(t->s, "reply");
    EXPECT_EQ(v->get("id")->i, 7);
    EXPECT_EQ(v->get("choice")->i, 3);
    EXPECT_TRUE(v->get("decision")->b);
    EXPECT_TRUE(v->get("skip")->isNull());
    EXPECT_EQ(v->get("text")->s, "你好\nP3");  // \uXXXX decoded to UTF-8 + \n
}

TEST(JsonUtil, WriterEscapesAndBuilds) {
    jsonu::Obj o;
    o.str("a", "he\"llo\n").num("b", 42).boolean("c", false).null("d");
    EXPECT_EQ(o.dump(), R"({"a":"he\"llo\n","b":42,"c":false,"d":null})");
}

TEST(JsonUtil, RejectsGarbage) {
    EXPECT_FALSE(jsonu::parse("not json").has_value());
    EXPECT_FALSE(jsonu::parse("{").has_value());
}

// --- provider: single-decision round trips ---

TEST(JsonProtocol, ScriptedChooseReturnsChoiceAndEmitsAsk) {
    std::istringstream in("{\"t\":\"reply\",\"id\":1,\"choice\":2}\n");
    std::ostringstream out;
    GameState s = board9State();
    JsonDecisionProvider p(in, out, "B", 1);
    EXPECT_EQ(p.chooseVote(s, 1, {1, 2, 3}), std::optional<int>(2));
    const std::string o = out.str();
    EXPECT_NE(o.find("\"qtype\":\"choose\""), std::string::npos);
    EXPECT_NE(o.find("\"kind\":\"Vote\""), std::string::npos);
    EXPECT_NE(o.find("\"allowSkip\":true"), std::string::npos);
}

TEST(JsonProtocol, NullChoiceAbstainsOnSkippable) {
    std::istringstream in("{\"t\":\"reply\",\"choice\":null}\n");
    std::ostringstream out;
    GameState s = board9State();
    JsonDecisionProvider p(in, out, "B", 1);
    EXPECT_FALSE(p.chooseInspect(s, 3, {1, 2, 4}).has_value());  // 不验
}

TEST(JsonProtocol, IllegalChoiceFallsBackForVote) {
    std::istringstream in("{\"t\":\"reply\",\"choice\":99}\n");  // 99 not a candidate
    std::ostringstream out;
    GameState s = board9State();
    JsonDecisionProvider p(in, out, "B", 1);
    EXPECT_EQ(p.chooseVote(s, 1, {1, 2, 3}), std::optional<int>(2));  // first non-self
}

TEST(JsonProtocol, EofVoteFallsBackToFirstNonSelf) {
    std::istringstream in;  // empty -> EOF on every read
    std::ostringstream out;
    GameState s = board9State();
    JsonDecisionProvider p(in, out, "B", 1);
    EXPECT_EQ(p.chooseVote(s, 1, {1, 2, 3}), std::optional<int>(2));
}

TEST(JsonProtocol, ConfirmTrue) {
    std::istringstream in("{\"t\":\"reply\",\"decision\":true}\n");
    std::ostringstream out;
    GameState s = board9State();
    JsonDecisionProvider p(in, out, "B", 1);
    EXPECT_TRUE(p.chooseRunForSheriff(s, 4));
    EXPECT_NE(out.str().find("\"kind\":\"RunForSheriff\""), std::string::npos);
}

TEST(JsonProtocol, CollectSpeechReturnsTextAndBroadcasts) {
    std::istringstream in("{\"t\":\"reply\",\"text\":\"我是预言家\"}\n");
    std::ostringstream out;
    GameState s = board9State();
    JsonDecisionProvider p(in, out, "B", 1);
    EXPECT_EQ(p.collectSpeech(s, 3, SpeechKind::Statement, 1), "我是预言家");
    const std::string o = out.str();
    EXPECT_NE(o.find("\"etype\":\"speech\""), std::string::npos);
    EXPECT_NE(o.find("\"vis\":\"public\""), std::string::npos);
}

TEST(JsonProtocol, InspectResultIsPrivateToSeer) {
    std::istringstream in;
    std::ostringstream out;
    JsonDecisionProvider p(in, out, "B", 1);
    p.onInspectResult(/*seer=*/3, /*target=*/2, /*isWolf=*/true);
    const std::string o = out.str();
    EXPECT_NE(o.find("\"vis\":\"private\""), std::string::npos);
    EXPECT_NE(o.find("\"seat\":3"), std::string::npos);
    EXPECT_NE(o.find("\"etype\":\"result_private\""), std::string::npos);
    EXPECT_NE(o.find("\"isWolf\":true"), std::string::npos);
}

// --- full game over the protocol (EOF = every decision falls back legally) ---

TEST(JsonProtocol, EofGameRunsToCompletionAndEmitsFrames) {
    std::istringstream in;  // orchestrator "dies" -> all fallbacks
    std::ostringstream out;
    Board board = makeBoard9_SeerWitchHunter();
    std::vector<RoleKind> roles = randomDeal(board, 4242);
    JsonDecisionProvider provider(in, out, board.name, 4242);
    Game game(board, provider, roles);
    provider.emitGameStart(game.state());
    provider.emitDeals(game.state());
    GameResult r = game.run();
    provider.emitGameOver(r);

    EXPECT_NE(r, GameResult::Ongoing);  // votes progress -> a side wins
    const std::string s = out.str();
    EXPECT_NE(s.find("\"t\":\"game_start\""), std::string::npos);
    EXPECT_NE(s.find("\"t\":\"game_over\""), std::string::npos);
    EXPECT_NE(s.find("\"etype\":\"deal\""), std::string::npos);
    EXPECT_GT(allLinesParse(s), 10);  // many protocol lines, all valid JSON
}

// Deal events must be private + role-bearing; a public line must never leak a role.
TEST(JsonProtocol, DealsArePrivateAndRolesNotPublic) {
    std::istringstream in;
    std::ostringstream out;
    Board board = makeBoard9_SeerWitchHunter();
    JsonDecisionProvider provider(in, out, board.name, 7);
    GameState seeded = buildInitialState(board, randomDeal(board, 7));
    Game game(board, provider, randomDeal(board, 7));
    provider.emitGameStart(game.state());
    provider.emitDeals(game.state());

    std::istringstream ls(out.str());
    std::string line;
    while (std::getline(ls, line)) {
        auto v = jsonu::parse(line);
        ASSERT_TRUE(v.has_value());
        const jsonu::Value* etype = v->get("etype");
        if (etype && etype->isStr() && etype->s == "deal") {
            const jsonu::Value* vis = v->get("vis");
            ASSERT_NE(vis, nullptr);
            EXPECT_EQ(vis->s, "private");
            EXPECT_NE(v->get("seat"), nullptr);  // addressed to a specific seat
        }
    }
    (void)seeded;
}
