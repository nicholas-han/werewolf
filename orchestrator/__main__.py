"""CLI: run one game.

    # all AI on local deepseek-r1:14b, you watch the god-script:
    python -m orchestrator --board 1 --seed 12345
    # you play seat 1, the rest are AI:
    python -m orchestrator --human-seat 1
    # deterministic, no model (for a quick pipeline check):
    python -m orchestrator --fake
"""

from __future__ import annotations

import argparse
import sys

from orchestrator.llm import OllamaClient
from orchestrator.runner import Config, run_game


def main() -> None:
    ap = argparse.ArgumentParser(description="Werewolf AI-agent orchestrator (M15).")
    ap.add_argument("--board", type=int, default=1, choices=[1, 2, 3])
    ap.add_argument("--seed", type=int, default=12345)
    ap.add_argument("--human-seat", type=int, default=None, help="座位号；省略=全部 AI")
    ap.add_argument("--provider", default="ollama", choices=["ollama", "fake"])
    ap.add_argument("--model", default="deepseek-r1:14b", help="Ollama 模型名")
    ap.add_argument("--ollama-host", default="http://localhost:11434")
    ap.add_argument("--timeout", type=int, default=600, help="单次模型调用软超时（秒）")
    ap.add_argument("--fake", action="store_true", help="等价于 --provider fake")
    ap.add_argument("--engine", default=None, help="werewolf 路径（默认 build/werewolf）")
    ap.add_argument("--out-dir", default=None, help="记录输出目录（默认 ./games）")
    args = ap.parse_args()

    provider = "fake" if args.fake else args.provider
    kw: dict = {
        "board": args.board, "seed": args.seed, "human_seat": args.human_seat,
        "provider": provider, "model": args.model, "ollama_host": args.ollama_host,
        "request_timeout": args.timeout,
    }
    if args.engine:
        kw["engine_path"] = args.engine
    if args.out_dir:
        kw["out_dir"] = args.out_dir
    cfg = Config(**kw)

    if cfg.provider == "ollama":
        ok, msg = OllamaClient(cfg.model, cfg.ollama_host).health()
        if not ok:
            print(f"[orchestrator] {msg}", file=sys.stderr)
            sys.exit(1)
        print(f"[orchestrator] 模型：{cfg.model} @ {cfg.ollama_host}（可能较慢，请耐心）", file=sys.stderr)

    orch = run_game(cfg)
    print(f"\n结果：{orch.result}")
    print(f"复盘 script：{cfg.out_dir}/god_script.md")
    print(f"训练 trace ：{cfg.out_dir}/trace.jsonl")


if __name__ == "__main__":
    main()
