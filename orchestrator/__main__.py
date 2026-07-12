"""CLI: run one game.

启动配置一条优先级链（见 orchestrator/config.py）：

    CLI 参数  >  werewolf.toml  >  启动交互问答  >  内置默认

**首次设置**（一次性，之后每次直接 `python -m orchestrator` 即开）：

    cp .env.example .env                  # 填入 API key（git-ignore，绝不提交）
    cp werewolf.toml.example werewolf.toml   # 填入偏好（provider/model/板子/座位…）

不建配置文件也行——缺什么启动时交互选什么（后端/模型/板子/座位都有选单），
或者全用 CLI 参数：

    python -m orchestrator --human-seat 1                      # 默认 deepseek-v4-flash
    python -m orchestrator --provider bailian --model qwen-max # 换厂商
    python -m orchestrator --provider ollama --human-seat 1    # 本地 Ollama（须先 ollama serve）
    python -m orchestrator --fake                              # 确定性假模型，秒级（无需 key）
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import urllib.request

from orchestrator.cloud import (GENERIC_PROVIDER, PROVIDER_PRESETS, build_client,
                                provider_choices)
from orchestrator.config import load_config, load_dotenv
from orchestrator.llm import OllamaClient
from orchestrator.runner import Config, run_game

_DEFAULT_MODEL = "deepseek-r1:14b"
_DEFAULT_OLLAMA_HOST = "http://localhost:11434"
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


def pick_provider() -> str:
    """Interactive backend menu; cloud entries show whether their key env is set."""
    ordered: list[str] = []
    seen = set()
    for name in ["deepseek"] + list(PROVIDER_PRESETS):
        preset = PROVIDER_PRESETS[name]
        if id(preset) in seen:  # skip aliases (bailian/dashscope share one preset)
            continue
        seen.add(id(preset))
        ordered.append(name)
    ordered += [GENERIC_PROVIDER, "ollama", "fake"]

    print("选择 AI 后端：")
    for i, name in enumerate(ordered, 1):
        preset = PROVIDER_PRESETS.get(name)
        if preset is not None:
            key_state = "已设置" if os.environ.get(preset.api_key_env, "").strip() else "未设置"
            print(f"  {i}) {preset.label}　[key: {preset.api_key_env} {key_state}]")
        elif name == GENERIC_PROVIDER:
            print(f"  {i}) 任意 OpenAI 兼容端点（需 --base-url / --api-key-env）")
        elif name == "ollama":
            print(f"  {i}) Ollama（本地，须先 ollama serve）")
        else:
            print(f"  {i}) fake（确定性假模型，测试/管线用，无需 key）")
    try:
        s = input(f"后端编号 [默认 1 = {ordered[0]}]> ").strip()
    except EOFError:
        s = ""
    try:
        idx = int(s)
        if 1 <= idx <= len(ordered):
            return ordered[idx - 1]
    except ValueError:
        pass
    return ordered[0]


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
    ap.add_argument("--config", default=None,
                    help="werewolf.toml 路径（默认仓库根；所有配置项可被 CLI 参数覆盖）")
    ap.add_argument("--board", type=int, default=None, choices=[1, 2, 3],
                    help="省略=配置文件/启动时选择")
    ap.add_argument("--seed", type=int, default=None, help="默认 12345")
    ap.add_argument("--human-seat", type=int, default=None,
                    help="座位号；0=纯 AI 观战；省略=配置文件/启动时选择")
    ap.add_argument("--provider", default=None,
                    choices=list(_LOCAL_PROVIDERS) + provider_choices(),
                    help="默认 deepseek(云端,需 DEEPSEEK_API_KEY)；ollama=本地；fake=假模型；"
                         "其余云端见 bailian/openai/moonshot/zhipu/anthropic/openai-compat；"
                         "省略且无配置时启动选单")
    ap.add_argument("--model", default=None,
                    help="模型名；省略 → Ollama 启动下拉 / 云端用该厂商默认模型")
    ap.add_argument("--ollama-host", default=None, help=f"默认 {_DEFAULT_OLLAMA_HOST}")
    ap.add_argument("--base-url", default=None,
                    help="云端：覆盖该厂商 base_url（provider=openai-compat 时必填）")
    ap.add_argument("--api-key-env", default=None,
                    help="云端：装 API key 的环境变量名（默认随厂商，如百炼=DASHSCOPE_API_KEY）")
    ap.add_argument("--json-object", action=argparse.BooleanOptionalAction, default=None,
                    help="云端：对 choose/confirm 下发 response_format=json_object（加强结构化）")
    ap.add_argument("--llm-timeout", type=int, default=None,
                    help="单次模型调用软超时（秒）；默认 本地 600 / 云端 60")
    ap.add_argument("--timeout", type=int, default=None,
                    help="（已改名 --llm-timeout，兼容保留）")
    ap.add_argument("--ask-timeout", type=int, default=None,
                    help="引擎等待每个 reply 的软超时（秒，<=0 关闭；默认 600，protocol §8）")
    ap.add_argument("--knowledge", default=None, choices=["auto", "full", "lite", "off"],
                    help="领域知识注入（issue #20）：auto=云端 full、本地 ollama 及 8k 小上下文"
                         "模型 lite（默认）；full=按角色/板子全量（约 12–15k 字符，适合云端大"
                         "上下文）；lite=精简；off=关")
    ap.add_argument("--wolf-chat-rounds", type=int, default=None,
                    help="狼队夜聊最大轮数（默认2，整轮全过则提前结束）")
    ap.add_argument("--interrupts", action=argparse.BooleanOptionalAction, default=None,
                    help="逐发言中断：每位发言后给自爆/退水机会（最忠实但慢；默认关）")
    ap.add_argument("--fake", action="store_true", help="等价于 --provider fake")
    ap.add_argument("--engine", default=None, help="werewolf 路径（默认 build/werewolf）")
    ap.add_argument("--out-dir", default=None, help="记录输出目录（默认 ./games）")
    args = ap.parse_args()

    # .env（API key）+ werewolf.toml（偏好）；真实环境变量 > .env，CLI > toml。
    loaded_env, shadowed_env = load_dotenv()
    conf = load_config(args.config)
    sources = []
    if loaded_env:
        sources.append(f".env（{', '.join(loaded_env)}）")
    if conf:
        sources.append(args.config or "werewolf.toml")
    if sources:
        print(f"[orchestrator] 已加载 {' | '.join(sources)}", file=sys.stderr)
    for name in shadowed_env:
        print(f"[orchestrator] 注意：{name} 使用的是终端环境变量，忽略了 .env 中的不同值"
              f"（如遇 401 请检查 shell 里是否残留旧 export）", file=sys.stderr)

    def pick(cli_value, key: str, default=None):
        return cli_value if cli_value is not None else conf.get(key, default)

    interactive = sys.stdin.isatty()  # only prompt in a real terminal

    provider = "fake" if args.fake else pick(args.provider, "provider")
    if provider is None:
        provider = pick_provider() if interactive else "deepseek"
    is_cloud = provider not in _LOCAL_PROVIDERS

    board = pick(args.board, "board")
    if board not in _BOARDS:
        if board is not None:
            print(f"[orchestrator] 配置里板子 {board} 无效（可选 1/2/3）", file=sys.stderr)
        board = pick_board() if interactive else 1

    ollama_host = pick(args.ollama_host, "ollama_host", _DEFAULT_OLLAMA_HOST)

    # 模型：ollama 走本地下拉/默认；云端用厂商默认（交互可下拉）；fake 无所谓。
    model = pick(args.model, "model")
    if provider == "ollama" and model is None:
        model = pick_model(ollama_host) if interactive else _DEFAULT_MODEL
    elif is_cloud and model is None and interactive:
        model = pick_cloud_model(provider)  # None → build_client 用预设默认

    # 座位：0 = 纯 AI 观战（配置文件里也用 0 表示，TOML 没有 null）。
    if args.human_seat is not None:
        human = args.human_seat or None
    elif "human_seat" in conf:
        human = conf["human_seat"] or None
    else:
        human = pick_human_seat(board) if interactive else None

    llm_timeout = args.llm_timeout
    if llm_timeout is None and args.timeout is not None:
        print("[orchestrator] --timeout 已改名 --llm-timeout（本次仍生效）", file=sys.stderr)
        llm_timeout = args.timeout
    if llm_timeout is None:
        llm_timeout = conf.get("timeout")
    if llm_timeout is None:
        llm_timeout = 600 if provider == "ollama" else 60

    knowledge = pick(args.knowledge, "knowledge", "auto")
    if knowledge not in ("auto", "full", "lite", "off"):  # toml 值不经 argparse choices
        print(f"[orchestrator] 配置里 knowledge={knowledge!r} 无效"
              "（可选 auto/full/lite/off），按 auto 处理", file=sys.stderr)
        knowledge = "auto"

    kw: dict = {
        "board": board, "seed": pick(args.seed, "seed", 12345), "human_seat": human,
        "provider": provider, "model": model, "ollama_host": ollama_host,
        "base_url": pick(args.base_url, "base_url"),
        "api_key_env": pick(args.api_key_env, "api_key_env"),
        "json_object": bool(pick(args.json_object, "json_object", False)),
        "request_timeout": llm_timeout,
        "ask_timeout": pick(args.ask_timeout, "ask_timeout", 600),
        "knowledge": knowledge,
        "wolf_chat_rounds": pick(args.wolf_chat_rounds, "wolf_chat_rounds", 2),
        "interrupts": bool(pick(args.interrupts, "interrupts", False)),
    }
    engine = pick(args.engine, "engine")
    if engine:
        kw["engine_path"] = engine
    out_dir = pick(args.out_dir, "out_dir")
    if out_dir:
        kw["out_dir"] = out_dir
    personas = conf.get("personas")
    if personas:
        kw["personas"] = personas
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
