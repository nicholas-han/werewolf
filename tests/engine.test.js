import test from "node:test";
import assert from "node:assert/strict";

import { buildEngine, packageRoot } from "../lib/engine.js";

test("buildEngine deduplicates only in-flight builds", { timeout: 200000 }, async () => {
  const first = buildEngine({ root: packageRoot });
  const second = buildEngine({ root: packageRoot });
  assert.equal(first, second, "concurrent callers must share one build");

  const binary = await first;
  assert.ok(binary.endsWith("werewolf") || binary.endsWith("werewolf.exe"));

  const later = buildEngine({ root: packageRoot });
  assert.notEqual(later, first, "a settled build must not be cached forever");
  await later;
});
