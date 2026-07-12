"""Launch configuration: `.env` auto-load + `werewolf.toml` preferences.

启动一局游戏的所有可调项此前散在三处：环境变量（手动 export API key）、CLI 参数、
启动后交互问答。本模块把它们收敛成一条清晰的优先级链：

    CLI 参数  >  werewolf.toml  >  启动交互问答  >  内置默认

- **`.env`**（仓库根，git-ignore）：装 API key 等环境变量，启动时自动载入，
  免去每开一个终端都要手动 export。已存在的真实环境变量**永远优先**。
- **`werewolf.toml`**（仓库根，git-ignore；模板见 `werewolf.toml.example`）：
  装个人偏好（provider/model/board/座位/超时/人设……）。安全原则同 `cloud.py`：
  配置只记「去哪个环境变量取 key」（`api_key_env`），**绝不装 key 本身**。
"""

from __future__ import annotations

import os
import pathlib
import sys
import tomllib

_REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]

DOTENV_PATH = _REPO_ROOT / ".env"
CONFIG_PATH = _REPO_ROOT / "werewolf.toml"


def load_dotenv(path: pathlib.Path | None = None) -> tuple[list[str], list[str]]:
    """Load KEY=VALUE lines from `.env` into os.environ.

    Returns `(loaded, shadowed)`: names taken from .env, and names whose .env
    value was IGNORED because the real environment already holds a *different*
    value（真实环境变量优先）。shadowed 用于启动提示——shell 里残留的旧
    export 会静默压过 .env 里的新 key，401 排查起来非常绕，必须点名。
    值相同的不算 shadowed（不是问题，提示只会变成噪音）。

    支持 `export KEY=...` 前缀、`#` 注释、引号包裹的值。文件不存在则安静返回
    空表（.env 是可选的）。
    """
    p = path if path is not None else DOTENV_PATH
    loaded: list[str] = []
    shadowed: list[str] = []
    if not p.is_file():
        return loaded, shadowed
    for raw in p.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("export "):
            line = line[len("export "):].lstrip()
        key, sep, val = line.partition("=")
        key, val = key.strip(), val.strip()
        if not sep or not key:
            continue
        if len(val) >= 2 and val[0] == val[-1] and val[0] in ("'", '"'):
            val = val[1:-1]
        if key not in os.environ:
            os.environ[key] = val
            loaded.append(key)
        elif val and os.environ[key] != val:
            shadowed.append(key)
    return loaded, shadowed


# werewolf.toml 的合法键（分节镜像 CLI 参数）。值是期望类型；bool 必须精确匹配
# （Python 里 True 是 int 的实例，不能让 bool 混进 int 项）。
_SCHEMA: dict[str, dict[str, type]] = {
    "game": {"board": int, "seed": int, "human_seat": int, "wolf_chat_rounds": int,
             "interrupts": bool, "ask_timeout": int},
    "llm": {"provider": str, "model": str, "timeout": int, "json_object": bool,
            "ollama_host": str, "base_url": str, "api_key_env": str, "knowledge": str},
    "paths": {"engine": str, "out_dir": str},
}


def _type_ok(value: object, expected: type) -> bool:
    if expected is int:
        return isinstance(value, int) and not isinstance(value, bool)
    return isinstance(value, expected)


def load_config(path: str | pathlib.Path | None = None) -> dict:
    """Parse werewolf.toml into a flat dict of known keys (+ `personas`: {seat: str}).

    显式给了 `path` 但文件不存在/解析失败 → 报错退出（用户点名要用它）；默认路径
    不存在 → 安静返回 {}（配置文件是可选的）。未知节/键、类型不对 → stderr 警告并
    跳过，不让一个手误废掉整个启动。
    """
    explicit = path is not None
    p = pathlib.Path(path) if explicit else CONFIG_PATH
    if not p.is_file():
        if explicit:
            print(f"[orchestrator] 配置文件不存在：{p}", file=sys.stderr)
            sys.exit(1)
        return {}
    try:
        with open(p, "rb") as f:
            data = tomllib.load(f)
    except (tomllib.TOMLDecodeError, OSError) as e:
        print(f"[orchestrator] 配置文件 {p} 解析失败：{e}", file=sys.stderr)
        sys.exit(1)

    flat: dict = {}
    for section, entries in data.items():
        if section == "personas":
            flat["personas"] = _parse_personas(entries, p)
            continue
        schema = _SCHEMA.get(section)
        if schema is None:
            print(f"[orchestrator] {p.name}: 未知配置节 [{section}]，忽略", file=sys.stderr)
            continue
        if not isinstance(entries, dict):
            print(f"[orchestrator] {p.name}: [{section}] 必须是配置节，忽略", file=sys.stderr)
            continue
        for key, value in entries.items():
            expected = schema.get(key)
            if expected is None:
                print(f"[orchestrator] {p.name}: 未知配置项 {section}.{key}，忽略",
                      file=sys.stderr)
                continue
            if not _type_ok(value, expected):
                print(f"[orchestrator] {p.name}: {section}.{key} 应为 {expected.__name__}，"
                      f"忽略非法值 {value!r}", file=sys.stderr)
                continue
            flat[key] = value
    return flat


def _parse_personas(entries: object, p: pathlib.Path) -> dict[int, str]:
    """[personas] 节：座位号（TOML 键是字符串）→ 人设文本。"""
    personas: dict[int, str] = {}
    if not isinstance(entries, dict):
        print(f"[orchestrator] {p.name}: [personas] 必须是配置节，忽略", file=sys.stderr)
        return personas
    for seat_key, text in entries.items():
        try:
            seat = int(seat_key)
        except ValueError:
            print(f"[orchestrator] {p.name}: personas 键须为座位号，忽略 {seat_key!r}",
                  file=sys.stderr)
            continue
        if not isinstance(text, str):
            print(f"[orchestrator] {p.name}: personas.{seat_key} 应为字符串，忽略",
                  file=sys.stderr)
            continue
        personas[seat] = text
    return personas
