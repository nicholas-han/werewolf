"""CLI: run one game.

**默认用云端 DeepSeek**（API key，M16）——本地推理模型太慢时的提速路径：

    export DEEPSEEK_API_KEY=sk-xxxx              # 先把 key 放进环境变量（绝不写进代码/配置）
    python -m orchestrator --human-seat 1                       # 默认 deepseek-v4-flash，你坐 1 号
    python -m orchestrator --model deepseek-v4-pro             # 更强的模型
    python -m orchestrator --fake                              # 确定性假模型，秒级（无需 key）

换其它厂商只改 --provider（key 各走自己的环境变量）：

    export DASHSCOPE_API_KEY=sk-xxxx
    python -m orchestrator --provider bailian --model qwen-max            # 阿里百炼
    python -m orchestrator --provider openai-compat \\
        --base-url https://your-endpoint/v1 --api-key-env MY_KEY --model foo   # 任意兼容端点

本地 Ollama（须先 ollama serve）：

    python -m orchestrator --provider ollama --human-seat 1    # 启动时下拉选本地模型
"""

from __future__ import annotations

import argparse
import json
import sys
import urllib.request

from orchestrator.cloud import (GENERIC_PROVIDER, PROVIDER_PRESETS, build_client,
                                provider_choices)
from orchestrator.llm import OllamaClient
from orchestrator.runner import Config, run_game

_DEFAULT_MODEL = "deepseek-r1:14b"
_BOARDS = {
    1: ("9 人 预女猎", 9),
    2: ("12 人 预女猎守 + 狼枪", 12),
    3: ("12 人 通灵机械狼", 12),
}
_LOCAL_PROVIDERS = ("ollama", "fake")  # 其余皆为云端（API key）


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
    """Interactive dropdown: list installed Ollama models, return the chosen one."""
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


def pick_cloud_model(provider: str) -> str | None:
    """Interactive dropdown for a cloud preset's suggested models (None = preset default).

    openai-compat 无预设 → 不下拉（须 --model）。"""
    preset = PROVIDER_PRESETS.get(provider)
    if preset is None or not preset.models:
        return None
    print(f"选择模型（{preset.label}）：")
    for i, m in enumerate(preset.models, 1):
        tag = "（默认）" if m == preset.default_model else ""
        print(f"  {i}) {m}{tag}")
    try:
        s = input(f"模型编号 [默认 = {preset.default_model}]> ").strip()
    except EOFError:
        s = ""
    if not s:
        return preset.default_model
    try:
        idx = int(s)
        if 1 <= idx <= len(preset.models):
            return preset.models[idx - 1]
    except ValueError:
        pass
    return preset.default_model


def main() -> None:
    ap = argparse.ArgumentParser(description="Werewolf AI-agent orchestrator (M15/M16).")
    ap.add_argument("--board", type=int, default=None, choices=[1, 2, 3], help="省略=启动时选择")
    ap.add_argument("--seed", type=int, default=12345)
    ap.add_argument("--human-seat", type=int, default=None, help="座位号；省略=启动时选择")
    ap.add_argument("--provider", default="deepseek",
                    choices=list(_LOCAL_PROVIDERS) + provider_choices(),
                    help="默认 deepseek(云端,需 DEEPSEEK_API_KEY)；ollama=本地；fake=假模型；"
                         "其余云端见 bailian/openai/moonshot/zhipu/anthropic/openai-compat")
    ap.add_argument("--model", default=None,
                    help="模型名；省略 → Ollama 启动下拉 / 云端用该厂商默认模型")
    ap.add_argument("--ollama-host", default="http://localhost:11434")
    ap.add_argument("--base-url", default=None,
                    help="云端：覆盖该厂商 base_url（provider=openai-compat 时必填）")
    ap.add_argument("--api-key-env", default=None,
                    help="云端：装 API key 的环境变量名（默认随厂商，如百炼=DASHSCOPE_API_KEY）")
    ap.add_argument("--json-object", action="store_true",
                    help="云端：对 choose/confirm 下发 response_format=json_object（加强结构化）")
    ap.add_argument("--timeout", type=int, default=None,
                    help="单次模型调用软超时（秒）；默认 本地 600 / 云端 60")
    ap.add_argument("--wolf-chat-rounds", type=int, default=2, help="狼队夜聊最大轮数（默认2，整轮全过则提前结束）")
    ap.add_argument("--interrupts", action="store_true", help="逐发言中断：每位发言后给自爆/退水机会（最忠实但慢；默认关）")
    ap.add_argument("--fake", action="store_true", help="等价于 --provider fake")
    ap.add_argument("--engine", default=None, help="werewolf 路径（默认 build/werewolf）")
    ap.add_argument("--out-dir", default=None, help="记录输出目录（默认 ./games）")
    args = ap.parse_args()

    provider = "fake" if args.fake else args.provider
    is_cloud = provider not in _LOCAL_PROVIDERS
    interactive = sys.stdin.isatty()  # only prompt in a real terminal

    board = args.board if args.board is not None else (pick_board() if interactive else 1)

    # 模型：ollama 走本地下拉/默认；云端用厂商默认（交互可下拉）；fake 无所谓。
    model = args.model
    if provider == "ollama" and model is None:
        model = pick_model(args.ollama_host) if interactive else _DEFAULT_MODEL
    elif is_cloud and model is None and interactive:
        model = pick_cloud_model(provider)  # None → build_client 用预设默认

    human = args.human_seat
    if human is None and interactive:
        human = pick_human_seat(board)

    timeout = args.timeout if args.timeout is not None else (600 if provider == "ollama" else 60)

    kw: dict = {
        "board": board, "seed": args.seed, "human_seat": human,
        "provider": provider, "model": model, "ollama_host": args.ollama_host,
        "base_url": args.base_url, "api_key_env": args.api_key_env,
        "json_object": args.json_object, "request_timeout": timeout,
        "wolf_chat_rounds": args.wolf_chat_rounds, "interrupts": args.interrupts,
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
    elif is_cloud:
        try:
            client = build_client(cfg.provider, model=cfg.model, base_url=cfg.base_url,
                                   api_key_env=cfg.api_key_env, timeout=cfg.request_timeout,
                                   json_object=cfg.json_object)
        except ValueError as e:
            print(f"[orchestrator] {e}", file=sys.stderr)
            sys.exit(1)
        ok, msg = client.health()
        if not ok:
            print(f"[orchestrator] {msg}", file=sys.stderr)
            sys.exit(1)
        print(f"[orchestrator] 云端模型 {client.name}（key 取自环境变量 "
              f"{client.api_key_env}）| 板子 {board} | 你的座位 "
              f"{human if human else '（纯 AI 观战）'}", file=sys.stderr)

    orch = run_game(cfg)
    print(f"\n结果：{orch.result}")
    print(f"本局目录：{orch.run_dir}")
    print(f"复盘 script：{orch.run_dir}/god_script.md")
    print(f"训练 trace ：{orch.run_dir}/trace.jsonl")


if __name__ == "__main__":
    main()
