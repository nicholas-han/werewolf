"""CLI: run one game. Step 4 uses FakeLlmClient for every AI seat.

    python -m orchestrator --board 1 --seed 12345 --human-seat 1

(Local deepseek-r1:32b / remote backends arrive in step 5; this entry will gain
a --model / config option then.)
"""

from __future__ import annotations

import argparse

from orchestrator.runner import Config, run_game


def main() -> None:
    ap = argparse.ArgumentParser(description="Werewolf AI-agent orchestrator (step 4: FakeLlmClient).")
    ap.add_argument("--board", type=int, default=1, choices=[1, 2, 3])
    ap.add_argument("--seed", type=int, default=12345)
    ap.add_argument("--human-seat", type=int, default=None, help="座位号；省略=全部 AI（无人类）")
    ap.add_argument("--engine", default=None, help="werewolf 可执行文件路径（默认 build/werewolf）")
    ap.add_argument("--out-dir", default=None, help="记录输出目录（默认 ./games）")
    args = ap.parse_args()

    kw: dict = {"board": args.board, "seed": args.seed, "human_seat": args.human_seat}
    if args.engine:
        kw["engine_path"] = args.engine
    if args.out_dir:
        kw["out_dir"] = args.out_dir
    cfg = Config(**kw)

    orch = run_game(cfg)
    print(f"\n结果：{orch.result}")
    print(f"复盘 script：{cfg.out_dir}/god_script.md")
    print(f"训练 trace ：{cfg.out_dir}/trace.jsonl")


if __name__ == "__main__":
    main()
