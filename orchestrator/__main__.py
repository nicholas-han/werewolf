"""CLI: run one game.

交互式启动（在真实终端里）会依次让你选 **板子 → 模型 → 你的座位**；任何一项都可用
对应 flag 预先指定来跳过：

    python -m orchestrator                       # 全程下拉选择
    python -m orchestrator --board 1 --human-seat 1 --model deepseek-r1:1.5b
    python -m orchestrator --fake                # 确定性假模型，秒级（管线/调试）
"""

from __future__ import annotations

import argparse
import json
import sys
import urllib.request

from orchestrator.llm import OllamaClient
from orchestrator.runner import Config, run_game

_DEFAULT_MODEL = "deepseek-r1:14b"
_BOARDS = {
    1: ("9 人 预女猎", 9),
    2: ("12 人 预女猎守 + 狼枪", 12),
    3: ("12 人 通灵机械狼", 12),
}


def pick_board() -> int:
    print("选择板子：")
    for k, (name, n) in _BOARDS.items():
        print(f"  {k}) {name}（{n} 人）")
    try:
        s = input("板子编号 [默认 1]> ").strip()
    except EOFError:
        s = ""
    try:
        n = int(s)
        if n in _BOARDS:
            return n
    except ValueError:
        pass
    return 1


def pick_human_seat(board: int) -> int | None:
    seats = _BOARDS.get(board, ("", 9))[1]
    try:
        s = input(f"你坐第几号座位？(1-{seats}，回车 = 纯 AI 观战)> ").strip()
    except EOFError:
        s = ""
    if not s:
        return None
    try:
        v = int(s)
        if 1 <= v <= seats:
            return v
    except ValueError:
        pass
    print("[orchestrator] 座位无效，按纯 AI 观战处理", file=sys.stderr)
    return None


def installed_models(host: str) -> list[str]:
    """Names of models installed in the local Ollama (empty on failure)."""
    try:
        with urllib.request.urlopen(host.rstrip("/") + "/api/tags", timeout=10) as r:
            data = json.loads(r.read().decode("utf-8"))
        models = [m.get("name", "") for m in data.get("models", []) if m.get("name")]
    except Exception:  # noqa: BLE001
        return []
    models.sort(key=lambda n: (not n.startswith("deepseek-r1"), n))  # r1 variants first
    return models


def pick_model(host: str) -> str:
    """Interactive dropdown: list installed models, return the chosen one."""
    models = installed_models(host)
    if not models:
        print(f"[orchestrator] 取模型列表失败，用默认 {_DEFAULT_MODEL}", file=sys.stderr)
        return _DEFAULT_MODEL
    print("选择模型（Ollama 本地）：")
    for i, m in enumerate(models, 1):
        print(f"  {i}) {m}")
    try:
        s = input(f"模型编号 [默认 1 = {models[0]}]> ").strip()
    except EOFError:
        s = ""
    if not s:
        return models[0]
    try:
        idx = int(s)
        if 1 <= idx <= len(models):
            return models[idx - 1]
    except ValueError:
        pass
    print(f"[orchestrator] 输入无效，用 {models[0]}", file=sys.stderr)
    return models[0]


def main() -> None:
    ap = argparse.ArgumentParser(description="Werewolf AI-agent orchestrator (M15).")
    ap.add_argument("--board", type=int, default=None, choices=[1, 2, 3], help="省略=启动时选择")
    ap.add_argument("--seed", type=int, default=12345)
    ap.add_argument("--human-seat", type=int, default=None, help="座位号；省略=启动时选择")
    ap.add_argument("--provider", default="ollama", choices=["ollama", "fake"])
    ap.add_argument("--model", default=None, help="Ollama 模型名；省略=启动时下拉选择")
    ap.add_argument("--ollama-host", default="http://localhost:11434")
    ap.add_argument("--timeout", type=int, default=600, help="单次模型调用软超时（秒）")
    ap.add_argument("--wolf-chat-rounds", type=int, default=2, help="狼队夜聊最大轮数（默认2，整轮全过则提前结束）")
    ap.add_argument("--fake", action="store_true", help="等价于 --provider fake")
    ap.add_argument("--engine", default=None, help="werewolf 路径（默认 build/werewolf）")
    ap.add_argument("--out-dir", default=None, help="记录输出目录（默认 ./games）")
    args = ap.parse_args()

    provider = "fake" if args.fake else args.provider
    interactive = sys.stdin.isatty()  # only prompt in a real terminal

    board = args.board if args.board is not None else (pick_board() if interactive else 1)

    model = args.model
    if provider == "ollama" and model is None:
        model = pick_model(args.ollama_host) if interactive else _DEFAULT_MODEL
    elif model is None:
        model = _DEFAULT_MODEL

    human = args.human_seat
    if human is None and interactive:
        human = pick_human_seat(board)

    kw: dict = {
        "board": board, "seed": args.seed, "human_seat": human,
        "provider": provider, "model": model, "ollama_host": args.ollama_host,
        "request_timeout": args.timeout, "wolf_chat_rounds": args.wolf_chat_rounds,
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
        print(f"[orchestrator] 板子 {board} | 模型 {cfg.model} | 你的座位 "
              f"{human if human else '（纯 AI 观战）'}（推理模型可能较慢）", file=sys.stderr)

    orch = run_game(cfg)
    print(f"\n结果：{orch.result}")
    print(f"复盘 script：{cfg.out_dir}/god_script.md")
    print(f"训练 trace ：{cfg.out_dir}/trace.jsonl")


if __name__ == "__main__":
    main()
