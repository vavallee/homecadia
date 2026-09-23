#!/usr/bin/env python3
"""Read a node through the Home Assistant matter-server WebSocket. One shot, bounded.

  tools/matter-node.py [get|interview] [NODE_ID]

get       -- the server's subscription cache; costs the device nothing.
interview -- forces a full attribute read on the device. Measured 2026-09-17 at
             ~150 mC per call, about ten minutes of the sensor's normal running
             (docs/field-notes.md section 22). Use it to prove liveness after a
             reflash, not as a poll.

Needs `pip install websockets`. URL and node default to the office setup.
"""
import asyncio
import json
import os
import sys
import time

import websockets

URL = os.environ.get("MATTER_WS", "ws://192.168.1.173:5580/ws")
MODE = sys.argv[1] if len(sys.argv) > 1 else "get"
NODE = int(sys.argv[2]) if len(sys.argv) > 2 else 25

WANT = {
    "0/40/10": "SoftwareVersionString",
    "0/51/1": "RebootCount",
    "1/1026/0": "Temp (x0.01 C)",
    "2/1029/0": "RH (x0.01 %)",
    "3/47/11": "BatVoltage mV",
    "3/47/12": "BatPercent (x0.5 %)",
}


async def call(ws, mid, command, **args):
    await ws.send(json.dumps({"message_id": mid, "command": command, "args": args}))
    while True:
        r = json.loads(await ws.recv())
        if r.get("message_id") == mid:
            return r


async def main():
    async with websockets.connect(URL, max_size=16 * 1024 * 1024) as ws:
        await ws.recv()  # server info frame
        if MODE == "interview":
            r = await call(ws, "1", "interview_node", node_id=NODE)
            if "error_code" in r:
                print("interview error:", r.get("details"))
                return 1
        r = await call(ws, "2", "get_node", node_id=NODE)
        node = r.get("result") or {}
        if "error_code" in r:
            print("get_node error:", r.get("details"))
            return 1
        print(time.strftime("%H:%M:%S"), "available:", node.get("available"),
              " last_interview:", node.get("last_interview"))
        attrs = node.get("attributes", {})
        for k, name in WANT.items():
            print(f"  {name:22s} {attrs.get(k)}")
    return 0


sys.exit(asyncio.run(asyncio.wait_for(main(), 90)))
