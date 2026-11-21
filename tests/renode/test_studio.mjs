import assert from "node:assert/strict";
import net from "node:net";
import {Duplex} from "node:stream";
import {once} from "node:events";
import {createInterface} from "node:readline/promises";
import {setTimeout as delay} from "node:timers/promises";
const sharedClient = process.env.ZEROSHIFT_SHARED_RPC;
if (!sharedClient) throw new Error("Set ZEROSHIFT_SHARED_RPC to the built ZeroShift rpc.mjs path.");
const {moduleSession} = await import(sharedClient);

const socket = net.connect(Number(process.argv[2]), "127.0.0.1");
await once(socket, "connect");
const transport = {...Duplex.toWeb(socket), abortController: new AbortController()};
const client = moduleSession(transport);
const monitor = createInterface({input: process.stdin, output: process.stdout});
async function reset(side) {
  await monitor.question("RESET " + side + "\n");
  await delay(1800);
}
const off = ["off", "off"];
const lowers = ["pmw3610", "stick", "encoder", "key", "tpd", "lpps", "pat9125", "off"];
const uppers = ["iqs", "off"];

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
  assert.deepEqual([state.lower, state.inner], saved);
  assert.deepEqual([state.activeLower, state.activeInner], active);
}
await delay(1200);
for (const side of ["left", "right"]) {
  const other = side === "left" ? "right" : "left";
  let previous = off;
  const start = await profile(side);
  check(start, off, off);
  assert.deepEqual(new Set(start.supported.lower), new Set(lowers));
  assert.deepEqual(new Set(start.supported.inner), new Set(uppers));
  for (const lower of lowers) {
    for (const upper of uppers) {
      const modes = [lower, upper];
      check(await profile(side, modes), modes, previous);
      await reset(side);
      const active = [lower === "pat9125" ? "off" : lower, upper];
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
