import assert from "node:assert/strict";
import net from "node:net";
import {Duplex} from "node:stream";
import {once} from "node:events";
import {createInterface} from "node:readline/promises";
import {setTimeout as delay} from "node:timers/promises";
const sharedClient = process.env.ZEROSHIFT_SHARED_RPC;
if (!sharedClient) throw new Error("Set ZEROSHIFT_SHARED_RPC to the built ZeroShift rpc.mjs path.");
const {moduleSession} = await import(sharedClient);

const Mode = {
  off: 0,
  pmw3610: 1,
  stick: 2,
  encoder: 3,
  key: 4,
  tpd: 5,
  iqs: 6,
  lpps: 7,
  pat9125: 8,
};

const socket = net.connect(Number(process.argv[2]), "127.0.0.1");
await once(socket, "connect");
const transport = {...Duplex.toWeb(socket), abortController: new AbortController()};
const client = moduleSession(transport);
const monitor = createInterface({input: process.stdin, output: process.stdout});
async function reset(side) {
  await monitor.question("RESET " + side + "\n");
  await delay(1800);
}
const off = [Mode.off, Mode.off];
const lowers = [Mode.pmw3610, Mode.stick, Mode.encoder, Mode.key, Mode.tpd, Mode.lpps, Mode.pat9125, Mode.off];
const uppers = [Mode.iqs, Mode.off];

async function profile(side, requested) {
  for (let attempt = 0; ; attempt++) {
    try {
      return await client.profile(side, requested);
    } catch (err) {
      if (requested || side !== "left" || !err.message.includes("左側の応答") || attempt >= 4) throw err;
      await delay(500);
    }
  }
}
function check(state, saved, active) {
  assert.equal(state.model, "Ananta");
  assert.deepEqual(state.slots.map(slot => slot.saved), saved);
  assert.deepEqual(state.slots.map(slot => slot.active), active);
}
await delay(1200);
for (const side of ["left", "right"]) {
  const other = side === "left" ? "right" : "left";
  let previous = off;
  const start = await profile(side);
  check(start, off, off);
  assert.deepEqual(new Set(start.slots[0].modes.map(([id]) => id)), new Set(lowers));
  assert.deepEqual(new Set(start.slots[1].modes.map(([id]) => id)), new Set(uppers));
  for (const lower of lowers) {
    for (const upper of uppers) {
      const modes = [lower, upper];
      check(await profile(side, modes), modes, previous);
      await reset(side);
      const active = [lower === Mode.pat9125 ? Mode.off : lower, upper];
      check(await profile(side), modes, active);
      check(await profile(other), off, off);
      console.log(side, lower, upper, "PASS");
      previous = active;
    }
  }
}
monitor.close();
process.stdin.pause();
await client.close();
socket.destroy();
console.log("32/32 reboot cases passed");
