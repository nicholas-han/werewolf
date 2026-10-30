import { spawn, spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readdirSync, statSync } from "node:fs";
import { join } from "node:path";
import { fileURLToPath } from "node:url";

// The rules engine is the C++ `werewolf --json` binary (docs/protocol_v1.md).
// The plugin does not reimplement the rules: it spawns that process and answers
// its asks. This module finds the binary, or builds it directly with the system
// C++ compiler when it is missing (no CMake required for the plugin itself).

export const packageRoot = fileURLToPath(new URL("../", import.meta.url));

const ENGINE_SOURCES = [
  "src/core/roles/role.cpp",
  "src/core/abilities/role_abilities.cpp",
  "src/core/board.cpp",
  "src/core/game_state.cpp",
  "src/flow/win_condition.cpp",
  "src/flow/settlement.cpp",
  "src/flow/paidao.cpp",
  "src/flow/game.cpp",
  "src/io/console_decision_provider.cpp",
  "src/io/pass_and_play_decision_provider.cpp",
  "src/io/routing_decision_provider.cpp",
  "src/io/bot_channel.cpp",
  "src/io/json_decision_provider.cpp",
  "src/app/main.cpp",
];

const executableName = process.platform === "win32" ? "werewolf.exe" : "werewolf";

let buildPromise = null;

export function enginePathCandidates(root = packageRoot) {
  return [
    process.env.WEREWOLF_ENGINE,
    join(root, "build", executableName),
    join(root, "bin", executableName),
  ].filter((value) => typeof value === "string" && value.length > 0);
}

export function findEngine(root = packageRoot) {
  for (const candidate of enginePathCandidates(root)) {
    try {
      if (statSync(candidate).isFile()) return candidate;
    } catch {
      // keep looking
    }
  }
  return null;
}

function newestSourceMtime(root) {
  let newest = 0;
  try {
    for (const relative of readdirSync(join(root, "src"), { recursive: true })) {
      const value = String(relative);
      if (!/\.(cpp|h)$/.test(value)) continue;
      try {
        newest = Math.max(newest, statSync(join(root, "src", value)).mtimeMs);
      } catch {
        // file vanished between readdir and stat; ignore
      }
    }
    if (newest > 0) return newest;
  } catch {
    // fall through to the explicit source list
  }

  for (const relative of ENGINE_SOURCES) {
    try {
      newest = Math.max(newest, statSync(join(root, relative)).mtimeMs);
    } catch {
      // A missing source means this checkout is incomplete; the compiler will
      // report it with a useful message.
    }
  }
  return newest;
}

function existingBinaryIsFresh(root, binary) {
  try {
    return statSync(binary).mtimeMs >= newestSourceMtime(root);
  } catch {
    return false;
  }
}

function findCompiler() {
  const candidates = [process.env.CXX, "c++", "clang++", "g++"].filter(Boolean);

  for (const candidate of candidates) {
    const probe = spawnSync(candidate, ["--version"], {
      encoding: "utf8",
      timeout: 15_000,
    });
    if (probe.status === 0) return candidate;
  }

  return null;
}

export function buildEngine({ root = packageRoot, onProgress } = {}) {
  if (buildPromise) return buildPromise;

  const inFlight = (async () => {
    const binary = join(root, "build", executableName);

    if (existsSync(binary) && existingBinaryIsFresh(root, binary)) {
      return binary;
    }

    const compiler = findCompiler();
    if (!compiler) {
      throw new Error(
        "找不到 C++ 编译器（c++ / clang++ / g++）。请先安装 Xcode Command Line Tools，" +
          "或设置 WEREWOLF_ENGINE 指向已编译的 werewolf 可执行文件。",
      );
    }

    onProgress?.({
      stage: "building",
      message: `正在编译规则引擎（${compiler}）…`,
    });

    mkdirSync(join(root, "build"), { recursive: true });

    const args = [
      "-std=c++20",
      "-O2",
      "-Isrc",
      "-pthread",
      ...ENGINE_SOURCES,
      "-o",
      binary,
    ];

    await new Promise((resolve, reject) => {
      const child = spawn(compiler, args, {
        cwd: root,
        stdio: ["ignore", "pipe", "pipe"],
      });
      let stderr = "";
      let stdout = "";
      let settled = false;
      const finish = (error) => {
        if (settled) return;
        settled = true;
        clearTimeout(timer);
        if (error) reject(error);
        else resolve();
      };
      const timer = setTimeout(() => {
        try {
          child.kill("SIGKILL");
        } catch {
          // already gone
        }
        finish(new Error("规则引擎编译超时（180 秒）。"));
      }, 180_000);

      child.stdout.on("data", (chunk) => {
        stdout += chunk.toString("utf8");
      });
      child.stderr.on("data", (chunk) => {
        stderr += chunk.toString("utf8");
      });
      child.on("error", (error) => finish(error));
      child.on("close", (code) => {
        if (code === 0) {
          finish();
          return;
        }
        const detail = (stderr || stdout).trim().split("\n").slice(-20).join("\n");
        finish(new Error(`规则引擎编译失败（${compiler}，exit ${code}）\n${detail}`));
      });
    });

    onProgress?.({ stage: "ready", message: "规则引擎已就绪。" });
    return binary;
  })();

  // Deduplicate only concurrent builds. Clearing on settle (success or failure)
  // lets a later source edit trigger a fresh build instead of reusing the old
  // resolved promise for the lifetime of the host.
  buildPromise = inFlight.finally(() => {
    buildPromise = null;
  });
  return buildPromise;
}

export async function ensureEngine({ root = packageRoot, onProgress } = {}) {
  const found = findEngine(root);
  if (found) {
    // A checked-in build/werewolf can go stale after a source edit; rebuild it
    // in place. An explicit WEREWOLF_ENGINE is always trusted as-is.
    const built = join(root, "build", executableName);
    if (found === built && !existingBinaryIsFresh(root, built)) {
      return buildEngine({ root, onProgress });
    }
    return found;
  }
  return buildEngine({ root, onProgress });
}

// Spawn the engine for one game. The protocol is a single linear stream:
// events/asks on stdout, one reply line per ask on stdin.
export function spawnEngine(engine, options) {
  const {
    board = 1,
    seed = 1,
    askTimeout = 0,
    wolfChatRounds = 2,
    interrupts = false,
  } = options || {};

  const args = [
    "--json",
    "--board",
    String(board),
    "--seed",
    String(seed),
    "--ask-timeout",
    String(askTimeout),
    "--wolf-chat-rounds",
    String(wolfChatRounds),
  ];

  if (interrupts) args.push("--interrupts");

  return spawn(engine, args, {
    stdio: ["pipe", "pipe", "pipe"],
    env: { ...process.env, LANG: "zh_CN.UTF-8", LC_ALL: "zh_CN.UTF-8" },
  });
}
