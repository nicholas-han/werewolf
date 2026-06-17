"""Werewolf AI-agent orchestrator (M15).

Drives the C++ engine over the JSON-lines protocol (docs/protocol_v1.md): owns
the human terminal + all AI brains + the model backend, routes per-seat events
and decisions, and records the god-view script + post-training trace.

The engine stays a pure rule authority; this package is the sidecar (BRD §10/§11,
roadmap §5 "AI Agent 玩家").
"""
