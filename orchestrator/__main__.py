"""CLI: run one game.

    # 交互选模型（列出已装 Ollama 模型）+ 你坐 1 号：
    python -m orchestrator --human-seat 1
    # 指定模型，跳过下拉：
    python -m orchestrator --model deepseek-r1:1.5b
    # 确定性假模型，秒级（管线检查/调试）：
    python -m orchestrator --fake
"""

from __future__ import annotations

import argparse
import json
import sys
import urllib.request

from orchestrator.llm import OllamaClient
from orchestrator.runner import Config, run_game

_DEFAULT_MODEL = "deepseek-r1:14b"


def installed_models(host: str) -> list[str]:
    """Names of models installed in the local Ollama (empty on failure)."""
    try:
        with urllib.request.urlopen(host.rstrip("/") + "/api/tags", timeout=10) as r:
            data = json.loads(r.read().decode("utf-8"))
        models = [m.get("name", "") for m in data.get("models", []) if m.get("name")]
    except Exception:  # noqa: BLE001
        return []
    # deepseek-r1 variants first (most relevant here), then alphabetical.
    models.sort(key=lambda n: (not n.startswith("deepseek-r1"), n))
    return models


def pick_model(host: str) -> str:
    """Interactive dropdown: list installed models, return the chosen one."""
    models = installed_models(host)
    if not models:
        print(f"[orchestrator] 取模型列表失败，用默认 {_DEFAULT_MODEL}", file=sys.stderr)
        return _DEFAULT_MODEL
    print("可用本地模型（Ollama）：")
    for i, m in enumerate(models, 1):
        print(f"  {i}) {m}")
    try:
        s = input(f"选择模型编号 [默认 1 = {models[0]}]> ").strip()
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
    ap.add_argument("--board", type=int, default=1, choices=[1, 2, 3])
    ap.add_argument("--seed", type=int, default=12345)
    ap.add_argument("--human-seat", type=int, default=None, help="座位号；省略=全部 AI")
    ap.add_argument("--provider", default="ollama", choices=["ollama", "fake"])
    ap.add_argument("--model", default=None, help="Ollama 模型名；省略=启动时下拉选择")
    ap.add_argument("--ollama-host", default="http://localhost:11434")
    ap.add_argument("--timeout", type=int, default=600, help="单次模型调用软超时（秒）")
    ap.add_argument("--fake", action="store_true", help="等价于 --provider fake")
    ap.add_argument("--engine", default=None, help="werewolf 路径（默认 build/werewolf）")
    ap.add_argument("--out-dir", default=None, help="记录输出目录（默认 ./games）")
    args = ap.parse_args()

    provider = "fake" if args.fake else args.provider
    model = args.model
    if provider == "ollama" and model is None:
        model = pick_model(args.ollama_host)  # 下拉选择
    elif model is None:
        model = _DEFAULT_MODEL

    kw: dict = {
        "board": args.board, "seed": args.seed, "human_seat": args.human_seat,
        "provider": provider, "model": model, "ollama_host": args.ollama_host,
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
        print(f"[orchestrator] 模型：{cfg.model} @ {cfg.ollama_host}（推理模型可能较慢，请耐心）",
              file=sys.stderr)

    orch = run_game(cfg)
    print(f"\n结果：{orch.result}")
    print(f"复盘 script：{cfg.out_dir}/god_script.md")
    print(f"训练 trace ：{cfg.out_dir}/trace.jsonl")


if __name__ == "__main__":
    main()
