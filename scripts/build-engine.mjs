#!/usr/bin/env node
// Build the C++ rules engine without requiring CMake.
//
// Usage: node scripts/build-engine.mjs [--force]
// Env:   CXX=clang++  (or c++/g++ auto-detected)

import { chmodSync, existsSync, mkdirSync, readdirSync, statSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..");
const executable = process.platform === "win32" ? "werewolf.exe" : "werewolf";
const output = join(root, "build", executable);

const sources = [
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

function findCompiler() {
  for (const candidate of [process.env.CXX, "c++", "clang++", "g++"].filter(Boolean)) {
    const probe = spawnSync(candidate, ["--version"], { encoding: "utf8", timeout: 15000 });
    if (probe.status === 0) return candidate;
  }
  return null;
}

function newestSourceMtime() {
  let newest = 0;
  try {
    for (const relative of readdirSync(join(root, "src"), { recursive: true })) {
      const value = String(relative);
      if (!/\.(cpp|h)$/.test(value)) continue;
      try {
        newest = Math.max(newest, statSync(join(root, "src", value)).mtimeMs);
      } catch {
        // ignore a file that vanished between readdir and stat
      }
    }
    if (newest > 0) return newest;
  } catch {
    // fall through
  }
  for (const source of sources) {
    try {
      newest = Math.max(newest, statSync(join(root, source)).mtimeMs);
    } catch {
      // compiler will report a genuinely missing source
    }
  }
  return newest;
}

const force = process.argv.includes("--force");

if (!force && existsSync(output) && statSync(output).mtimeMs >= newestSourceMtime()) {
  console.log(`[werewolf] engine is up to date: ${output}`);
  process.exit(0);
}

const compiler = findCompiler();
if (!compiler) {
  console.error(
    "[werewolf] no C++ compiler found. Install Xcode Command Line Tools (macOS) or g++ (Linux), " +
      "or set WEREWOLF_ENGINE to a prebuilt binary.",
  );
  process.exit(1);
}

mkdirSync(join(root, "build"), { recursive: true });
console.log(`[werewolf] building ${output} with ${compiler}`);

const result = spawnSync(
  compiler,
  [
    "-std=c++20",
    "-O2",
    "-Isrc",
    "-pthread",
    ...sources,
    "-o",
    output,
  ],
  { cwd: root, stdio: "inherit", timeout: 180000 },
);

if (result.status !== 0) {
  console.error(`[werewolf] build failed (exit ${result.status})`);
  process.exit(result.status || 1);
}

try {
  chmodSync(output, 0o755);
} catch {
  // best effort
}

console.log(`[werewolf] built ${output}`);
