import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

import { apply } from "../index.js";

function mockContext() {
  const routes = [];
  const tools = [];
  const disposers = [];

  const ctx = {
    connection: { requestRejection: () => undefined },
    webServer: {
      register(route) {
        routes.push(route);
        return () => {};
      },
    },
    tools: {
      register(tool) {
        tools.push(tool);
        return () => {};
      },
    },
    effect(factory) {
      const disposer = factory();
      if (typeof disposer === "function") disposers.push(disposer);
    },
  };

  return { ctx, routes, tools, disposers };
}

function mockResponse() {
  const res = {
    status: 0,
    headers: null,
    body: "",
    writeHead(status, headers) {
      this.status = status;
      this.headers = headers;
    },
    end(body) {
      this.body = body || "";
    },
  };
  return res;
}

async function request(handler, method, url, body) {
  const req = {
    method,
    url,
    headers: {},
    async *[Symbol.asyncIterator]() {
      if (body !== undefined) yield Buffer.from(JSON.stringify(body), "utf8");
    },
  };
  const res = mockResponse();
  await handler(req, res);
  let json = null;
  try {
    json = JSON.parse(res.body);
  } catch {
    // not JSON
  }
  return { status: res.status, json };
}

test("host plugin registers routes and tools and serves a game", { timeout: 90000 }, async () => {
  const home = mkdtempSync(join(tmpdir(), "werewolf-http-"));
  const previousHome = process.env.DSH_HOME;
  process.env.DSH_HOME = home;
  const { ctx, routes, tools, disposers } = mockContext();
  const runtime = apply(ctx);
  try {
    assert.equal(routes.length, 1);
    assert.equal(routes[0].path, "/werewolf");
    assert.equal(tools.length, 1);
    assert.equal(tools[0].name, "werewolf");

    const idle = await request(routes[0].handler, "GET", "/werewolf/state");
    assert.equal(idle.status, 200);
    assert.equal(idle.json.ok, true);
    assert.equal(idle.json.state.status, "idle");
    assert.equal(idle.json.state.board.id, 1);

    const started = await request(routes[0].handler, "POST", "/werewolf/action", {
      action: "new",
      board: 1,
      humanSeat: 0,
      speed: "fast",
      seed: 21,
    });
    assert.equal(started.status, 200, started.json?.error);
    assert.ok(["starting", "building", "running"].includes(started.json.state.status));

    const state = await request(routes[0].handler, "GET", "/werewolf/state");
    assert.equal(state.status, 200);
    assert.equal(state.json.state.board.id, 1);

    const toolState = await tools[0].execute({ action: "state" });
    assert.equal(toolState.ok, true);
    assert.match(toolState.text, /Werewolf|狼人杀|Day|第/);

    const aborted = await request(routes[0].handler, "POST", "/werewolf/action", {
      action: "abort",
    });
    assert.equal(aborted.status, 200);
    assert.ok(["idle", "aborted", "finished", "running"].includes(aborted.json.state.status));
  } finally {
    runtime.dispose();
    for (const dispose of disposers) dispose();
    if (previousHome === undefined) delete process.env.DSH_HOME;
    else process.env.DSH_HOME = previousHome;
    rmSync(home, { recursive: true, force: true });
  }
});

test("tool exposes events and requires the observed ask identifiers", { timeout: 90000 }, async () => {
  const home = mkdtempSync(join(tmpdir(), "werewolf-http-"));
  const previousHome = process.env.DSH_HOME;
  process.env.DSH_HOME = home;
  const { ctx, tools } = mockContext();
  const runtime = apply(ctx);
  try {
    await tools[0].execute({
      action: "new",
      board: 1,
      humanSeat: 1,
      speed: "fast",
      seed: 1234,
    });

    const started = Date.now();
    while (Date.now() - started < 60000) {
      if (runtime.snapshot().pendingAsk) break;
      if (["finished", "error"].includes(runtime.snapshot().status)) break;
      await new Promise((resolve) => setTimeout(resolve, 40));
    }

    const state = await tools[0].execute({ action: "state" });
    assert.equal(state.ok, true);
    assert.equal(typeof state.sessionId, "string");
    assert.ok(Array.isArray(state.events));
    assert.ok(state.pendingAsk, "human seat should have a pending ask");
    assert.match(state.text, /Recent events:/);
    assert.ok(state.text.includes(state.sessionId), "tool text must expose the session id");

    const { id, qtype, candidates } = state.pendingAsk;
    await assert.rejects(
      () => tools[0].execute({ action: "reply", choice: null }),
      /sessionId|askId/,
    );

    const reply = { action: "reply", sessionId: state.sessionId, askId: id };
    if (qtype === "choose") reply.choice = candidates[0]?.seat ?? null;
    else if (qtype === "confirm") reply.decision = false;
    else reply.text = "";

    const answered = await tools[0].execute(reply);
    assert.equal(answered.ok, true);

    await assert.rejects(
      () => tools[0].execute(reply),
      /过期|没有等待|对局已经更换|no pending ask/i,
    );
  } finally {
    runtime.dispose();
    if (previousHome === undefined) delete process.env.DSH_HOME;
    else process.env.DSH_HOME = previousHome;
    rmSync(home, { recursive: true, force: true });
  }
});

test("action validation rejects unknown actions", { timeout: 30000 }, async () => {
  const home = mkdtempSync(join(tmpdir(), "werewolf-http-"));
  const previousHome = process.env.DSH_HOME;
  process.env.DSH_HOME = home;
  const { ctx, routes } = mockContext();
  const runtime = apply(ctx);
  try {
    const bad = await request(routes[0].handler, "POST", "/werewolf/action", {
      action: "definitely-not-a-real-action",
    });
    assert.equal(bad.status, 400);
    assert.equal(bad.json.ok, false);
    assert.match(bad.json.error, /未知操作/);
  } finally {
    runtime.dispose();
    if (previousHome === undefined) delete process.env.DSH_HOME;
    else process.env.DSH_HOME = previousHome;
    rmSync(home, { recursive: true, force: true });
  }
});
