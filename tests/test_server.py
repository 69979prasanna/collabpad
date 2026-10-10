#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# PYTHONIOENCODING=utf-8
"""
tests/test_server.py
--------------------
Phase 3 integration test for the CollabPad WebSocket server.

Requirements:
  Python 3.7+  (already confirmed available: Python 3.11.0)
  websockets   (already confirmed available: 17.1)
    Install if missing: pip install websockets

Usage (run from the repo root with the server already running):
  python tests/test_server.py [port]

The test:
  1. Connects Client A and Client B to document "demo".
  2. Connects Client C to document "other" (a different document).
  3. Sends an operation from Client A.
  4. Verifies Client B receives the forwarded message.
  5. Verifies Client A does NOT receive its own message back.
  6. Verifies Client C does NOT receive the "demo" message.
  7. Disconnects Client A and verifies the server is still alive.
  8. Reports PASS / FAIL per step and exits with code 0 if all pass.
"""
import os
os.environ.setdefault('PYTHONIOENCODING', 'utf-8')

import asyncio
import json
import sys
import time
import io

# Force UTF-8 output so checkmark characters work on Windows consoles.
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

try:
    import websockets
    import websockets.exceptions
except ImportError:
    print("ERROR: 'websockets' package not found.")
    print("Install it with:  pip install websockets")
    sys.exit(2)

# ------------------------------------------------------------------
# Config
# ------------------------------------------------------------------
PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
URI  = f"ws://127.0.0.1:{PORT}"

PASS = "\033[32mPASS\033[0m"
FAIL = "\033[31mFAIL\033[0m"

failures = 0

def has_fields(msg_str: str, **fields) -> bool:
    """Parse msg_str as JSON and check that each key has the given value."""
    try:
        obj = json.loads(msg_str)
        for k, v in fields.items():
            if obj.get(k) != v:
                return False
        return True
    except Exception:
        return False

def check(label: str, condition: bool, detail: str = ""):
    global failures
    status = PASS if condition else FAIL
    print("  [" + status + "] " + label + (" -- " + detail if detail else ""))
    if not condition:
        failures += 1

# ------------------------------------------------------------------
# Async receive helper with timeout
# ------------------------------------------------------------------
async def try_recv(ws, timeout=1.5):
    """Try to receive one message.  Returns None on timeout."""
    try:
        return await asyncio.wait_for(ws.recv(), timeout=timeout)
    except asyncio.TimeoutError:
        return None
    except Exception:
        return None

# ------------------------------------------------------------------
# Main test coroutine
# ------------------------------------------------------------------
async def run_tests():
    global failures

    print(f"\nCollabPad Phase 3 — WebSocket Server Test")
    print(f"Connecting to {URI}\n")

    # ------------------------------------------------------------------
    # Step 0: Wait for server to be reachable (retry a few times)
    # ------------------------------------------------------------------
    for attempt in range(5):
        try:
            async with websockets.connect(URI):
                pass
            break
        except Exception as e:
            if attempt == 4:
                print(f"FATAL: Cannot connect to server at {URI}: {e}")
                print("Make sure the server is running first:")
                print(f"  .\\build\\collabpad_server.exe {PORT}")
                sys.exit(1)
            await asyncio.sleep(0.5)

    # ------------------------------------------------------------------
    # Step 1: Connect Client A and Client B to "demo"; Client C to "other"
    # ------------------------------------------------------------------
    print("=== Step 1: Connect clients ===")
    async with websockets.connect(URI) as clientA, \
               websockets.connect(URI) as clientB, \
               websockets.connect(URI) as clientC:

        # Join messages
        join_demo_a = json.dumps({"type": "join", "docId": "demo", "siteId": 1, "name": "Alice"})
        join_demo_b = json.dumps({"type": "join", "docId": "demo", "siteId": 2, "name": "Bob"})
        join_other_c = json.dumps({"type": "join", "docId": "other", "siteId": 3, "name": "Carol"})

        await clientA.send(join_demo_a)
        ack_a = await try_recv(clientA, timeout=2.0)
        check("Client A joins 'demo' and receives ack",
              ack_a is not None and has_fields(ack_a, type="joined", docId="demo"),
              f"got: {ack_a!r}")

        await clientB.send(join_demo_b)
        ack_b = await try_recv(clientB, timeout=2.0)
        check("Client B joins 'demo' and receives ack",
              ack_b is not None and has_fields(ack_b, type="joined", docId="demo"),
              f"got: {ack_b!r}")

        await clientC.send(join_other_c)
        ack_c = await try_recv(clientC, timeout=2.0)
        check("Client C joins 'other' and receives ack",
              ack_c is not None and has_fields(ack_c, type="joined", docId="other"),
              f"got: {ack_c!r}")

        # ------------------------------------------------------------------
        # Step 2: Client A sends an operation
        # ------------------------------------------------------------------
        print("\n=== Step 2: Client A sends an operation ===")
        op = {
            "type": "op",
            "docId": "demo",
            "siteId": 1,
            "op": {
                "id":      {"siteId": 1, "clock": 1},
                "afterId": {"siteId": 0, "clock": 0},
                "value":   "H"
            }
        }
        op_json = json.dumps(op)
        await clientA.send(op_json)

        # ------------------------------------------------------------------
        # Step 3: Client B should receive the operation
        # ------------------------------------------------------------------
        print("\n=== Step 3: Verify Client B receives the operation ===")
        msg_b = await try_recv(clientB, timeout=2.0)
        check("Client B receives the forwarded operation",
              msg_b is not None and has_fields(msg_b, type="op", docId="demo"),
              f"got: {msg_b!r}")

        # ------------------------------------------------------------------
        # Step 4: Client A should NOT receive its own operation back
        # ------------------------------------------------------------------
        print("\n=== Step 4: Verify Client A does NOT receive its own op ===")
        echo_a = await try_recv(clientA, timeout=1.0)
        check("Client A does NOT receive echo of its own op",
              echo_a is None,
              f"unexpectedly got: {echo_a!r}")

        # ------------------------------------------------------------------
        # Step 5: Client C (different doc) should NOT receive the operation
        # ------------------------------------------------------------------
        print("\n=== Step 5: Verify Client C (different doc) receives nothing ===")
        msg_c = await try_recv(clientC, timeout=1.0)
        check("Client C does NOT receive the 'demo' operation",
              msg_c is None,
              f"unexpectedly got: {msg_c!r}")

        # ------------------------------------------------------------------
        # Step 6: Disconnect Client A; server stays alive
        # ------------------------------------------------------------------
        print("\n=== Step 6: Disconnect Client A; verify server is still up ===")
        await clientA.close()
        await asyncio.sleep(0.3)   # give server time to notice the close

        # Client B sends another op — if the server is alive it will be processed
        op2 = {
            "type": "op",
            "docId": "demo",
            "siteId": 2,
            "op": {
                "id":      {"siteId": 2, "clock": 1},
                "afterId": {"siteId": 1, "clock": 1},
                "value":   "i"
            }
        }
        try:
            await clientB.send(json.dumps(op2))
            # No-one else is in "demo" to receive it (A left), that's fine.
            # The point is the send didn't throw — the server is still running.
            check("Server still accepts messages after Client A disconnected", True)
        except Exception as e:
            check("Server still accepts messages after Client A disconnected",
                  False, str(e))

        # ------------------------------------------------------------------
        # Step 7: Verify Client B can still receive a new broadcast
        #         (connect a new Client D to "demo" and send from B)
        # ------------------------------------------------------------------
        print("\n=== Step 7: New client D connects to 'demo' and receives broadcast ===")
        async with websockets.connect(URI) as clientD:
            await clientD.send(json.dumps({"type": "join", "docId": "demo", "siteId": 4, "name": "Dave"}))
            ack_d = await try_recv(clientD, timeout=2.0)
            check("Client D joins 'demo' and receives ack",
                  ack_d is not None and has_fields(ack_d, type="joined", docId="demo"),
                  f"got: {ack_d!r}")

            # B sends op, D should receive it
            op3 = {"type": "op", "docId": "demo", "siteId": 2,
                   "op": {"id": {"siteId": 2, "clock": 2},
                          "afterId": {"siteId": 2, "clock": 1}, "value": "!"}}
            await clientB.send(json.dumps(op3))
            msg_d = await try_recv(clientD, timeout=2.0)
            check("Client D receives op from Client B",
                  msg_d is not None and has_fields(msg_d, type="op", docId="demo"),
                  f"got: {msg_d!r}")

    # ------------------------------------------------------------------
    # Summary
    # ------------------------------------------------------------------
    print("\n" + "=" * 45)
    if failures == 0:
        print("  All tests PASSED [OK]")
    else:
        print("  " + str(failures) + " test(s) FAILED [!!]")
    print("=" * 45 + "\n")
    return failures

# ------------------------------------------------------------------
# Entry point
# ------------------------------------------------------------------
if __name__ == "__main__":
    result = asyncio.run(run_tests())
    sys.exit(0 if result == 0 else 1)
