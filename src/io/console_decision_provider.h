#pragma once

#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "io/decision_provider.h"

// ConsoleDecisionProvider drives the engine from a terminal (BRD §10/M4). It is
// the human-facing implementation: every decision is a prompt on `out_`, every
// answer a line on `in_`.
//
// Single-terminal limitation (BRD §11): true per-player secrecy is impossible on
// one shared screen, so this is a moderator-operated, pass-and-play console. The
// streams are injectable for testing.
namespace ww {

class ConsoleDecisionProvider : public DecisionProvider {
public:
    ConsoleDecisionProvider(std::istream& in, std::ostream& out);

    std::optional<int> chooseNightKill(const GameState&, const std::vector<int>&) override;
    std::optional<int> chooseVote(const GameState&, int, const std::vector<int>&) override;
    std::optional<int> chooseInspect(const GameState&, int, const std::vector<int>&) override;
    bool chooseWitchSave(const GameState&, int, int) override;
    std::optional<int> chooseWitchPoison(const GameState&, int, const std::vector<int>&) override;
    std::optional<int> chooseHunterShot(const GameState&, int, const std::vector<int>&) override;
    std::optional<int> chooseSelfDestruct(const GameState&, const std::vector<int>&) override;
    bool chooseRunForSheriff(const GameState&, int) override;
    bool chooseWithdraw(const GameState&, int) override;
    std::optional<int> chooseSheriffVote(const GameState&, int, const std::vector<int>&) override;
    SheriffBallot chooseSheriffExileBallot(const GameState&, int, const std::vector<int>&) override;
    std::optional<int> chooseBadgeTransfer(const GameState&, int, const std::vector<int>&) override;
    SpeechDirection chooseSpeechDirection(const GameState&, int, int, bool) override;
    void onInspectResult(int seerId, int targetId, bool isWolf) override;
    void notify(const std::string& message) override;
    void pause(const std::string& note) override;

private:
    std::istream& in_;
    std::ostream& out_;

    // Reads one trimmed line; std::nullopt on EOF.
    std::optional<std::string> readLine();
    // Prompt for an optional candidate (blank = std::nullopt). Re-prompts on bad input.
    std::optional<int> promptOptional(const std::string& prompt, const GameState&,
                                      const std::vector<int>& candidates);
    // Prompt for a required candidate (re-prompts; falls back to first on EOF).
    int promptRequired(const std::string& prompt, const GameState&,
                       const std::vector<int>& candidates);
    bool promptYesNo(const std::string& prompt);
    void listCandidates(const GameState&, const std::vector<int>& candidates);
    std::string nameOf(const GameState&, int id);
};

}  // namespace ww
