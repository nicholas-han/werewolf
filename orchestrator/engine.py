"""EngineProcess — spawn the C++ engine and speak the JSON-lines protocol."""

from __future__ import annotations

import json
import subprocess
from typing import Iterator


class EngineProcess:
    """Launches `werewolf --json` and exchanges newline-delimited JSON.

    The engine is single-threaded and synchronous: it emits events/asks on
    stdout and blocks on stdin for a `reply` after each `ask`. We iterate stdout
    and send replies inline, so there is no concurrency to manage.
    """

    def __init__(self, engine_path: str, board: int, seed: int, ask_timeout: int = 600):
        self.proc = subprocess.Popen(
            [engine_path, "--json", "--board", str(board), "--seed", str(seed),
             "--ask-timeout", str(ask_timeout)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            bufsize=1,
        )

    def messages(self) -> Iterator[dict]:
        """Yield each protocol message (one JSON object per stdout line)."""
        assert self.proc.stdout is not None
        for line in self.proc.stdout:
            line = line.strip()
            if not line:
                continue
            try:
                yield json.loads(line)
            except json.JSONDecodeError:
                # The engine should only emit valid JSON on stdout; skip noise.
                continue

    def send(self, obj: dict) -> None:
        """Send one reply (UTF-8, raw; the engine parses raw bytes and \\uXXXX)."""
        assert self.proc.stdin is not None
        self.proc.stdin.write(json.dumps(obj, ensure_ascii=False) + "\n")
        self.proc.stdin.flush()

    def stderr_text(self) -> str:
        assert self.proc.stderr is not None
        try:
            return self.proc.stderr.read()
        except Exception:
            return ""

    def close(self) -> int:
        try:
            if self.proc.stdin and not self.proc.stdin.closed:
                self.proc.stdin.close()  # EOF -> engine finishes
        except Exception:
            pass
        try:
            code = self.proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            code = self.proc.wait()
        for stream in (self.proc.stdout, self.proc.stderr):
            try:
                if stream and not stream.closed:
                    stream.close()
            except Exception:
                pass
        return code
