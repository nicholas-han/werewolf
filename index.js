import { fileURLToPath } from "node:url";

import { WerewolfRuntime } from "./lib/runtime.js";
import { registerWerewolfTools } from "./lib/tools.js";

export const name = "werewolf";
export const inject = ["webServer", "connection", "tools"];

const packageRoot = fileURLToPath(new URL("./", import.meta.url));
const MAX_BODY = 128 * 1024;

function sendJson(res, status, value) {
  const body = JSON.stringify(value);
  res.writeHead(status, {
    "content-type": "application/json; charset=utf-8",
    "content-length": Buffer.byteLength(body),
    "cache-control": "no-store",
    "content-security-policy": "default-src 'none'",
    "x-content-type-options": "nosniff",
  });
  res.end(body);
}

async function readJson(req) {
  const chunks = [];
  let size = 0;

  for await (const chunk of req) {
    size += chunk.length;
    if (size > MAX_BODY) throw new Error("请求内容过大。");
    chunks.push(chunk);
  }

  if (size === 0) return {};

  try {
    return JSON.parse(Buffer.concat(chunks).toString("utf8"));
  } catch {
    throw new Error("请求内容必须是合法 JSON。");
  }
}

async function handle(ctx, runtime, req, res) {
  try {
    const rejection = ctx.connection.requestRejection(req);
    if (rejection !== undefined) {
      res.writeHead(rejection);
      res.end();
      return;
    }

    const url = new URL(req.url || "/", "http://localhost");
    const path = url.pathname.slice("/werewolf".length);

    if (path === "/state" && req.method === "GET") {
      sendJson(res, 200, { ok: true, state: runtime.snapshot() });
      return;
    }

    if (path === "/action" && req.method === "POST") {
      const body = await readJson(req);
      const action = typeof body?.action === "string" ? body.action : "";

      let state;
      switch (action) {
        case "new":
          state = runtime.startGame(body);
          break;
        case "reply":
          state = runtime.answer(body);
          break;
        case "auto":
          state = runtime.autoAnswer(body);
          break;
        case "speed":
          state = runtime.setSpeed(body.speed);
          break;
        case "abort":
          state = runtime.abort();
          break;
        default:
          throw new Error(`未知操作：${action || "(missing)"}`);
      }

      sendJson(res, 200, { ok: true, state });
      return;
    }

    sendJson(res, 404, { ok: false, error: "Not found." });
  } catch (error) {
    sendJson(res, 400, {
      ok: false,
      error: error instanceof Error ? error.message : String(error),
    });
  }
}

export function apply(ctx) {
  const runtime = new WerewolfRuntime({ root: packageRoot });

  ctx.effect(() => () => runtime.dispose(), "werewolf: lifecycle");

  ctx.effect(
    () =>
      ctx.webServer.register({
        kind: "prefix",
        path: "/werewolf",
        handler: (req, res) => handle(ctx, runtime, req, res),
      }),
    "werewolf: routes",
  );

  registerWerewolfTools(ctx, runtime);

  return runtime;
}
