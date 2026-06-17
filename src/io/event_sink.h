#pragma once

#include <optional>
#include <ostream>
#include <string>

#include "io/json_util.h"

// EventSink decouples "what to say + to whom (vis)" from "how to transport it"
// (docs/protocol_v1.md §6, ai_agents_design.md §5.2). The engine's JSON provider
// emits structured Events; JsonEventSink writes them as one JSON line each.
namespace ww {

// Information visibility (BRD §11): public = everyone, private = one seat,
// moderator = god-view only (never to any player/brain).
enum class Vis { Public, Private, Moderator };

inline const char* visName(Vis v) {
    switch (v) {
        case Vis::Public: return "public";
        case Vis::Private: return "private";
        case Vis::Moderator: return "moderator";
    }
    return "public";
}

struct Event {
    Vis vis = Vis::Public;
    std::optional<int> seat;   // required when vis == Private (the recipient)
    std::string etype;         // deal/phase/narration/speech/death/vote/result_private/decision/status
    std::string text;          // human-facing Chinese (rendered via messages.h)
    std::optional<int> day;
    std::string phase;         // "Night"/"Day"/"" (omitted when empty)
    std::string dataJson;      // pre-serialized JSON object, "" = omit
};

class EventSink {
public:
    virtual ~EventSink() = default;
    virtual void emit(const Event& e) = 0;
};

// Writes each Event as one JSON line to the protocol stream.
class JsonEventSink : public EventSink {
public:
    explicit JsonEventSink(std::ostream& out) : out_(out) {}

    void emit(const Event& e) override {
        jsonu::Obj o;
        o.str("t", "event").str("vis", visName(e.vis));
        if (e.seat) o.num("seat", *e.seat);
        o.str("etype", e.etype);
        o.str("text", e.text);
        if (e.day) o.num("day", *e.day);
        if (!e.phase.empty()) o.str("phase", e.phase);
        if (!e.dataJson.empty()) o.raw("data", e.dataJson);
        out_ << o.dump() << "\n";
        out_.flush();
    }

private:
    std::ostream& out_;
};

}  // namespace ww
