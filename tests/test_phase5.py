#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# PYTHONIOENCODING=utf-8
"""
tests/test_phase5.py
--------------------
Phase 5 Comprehensive Integration & Regression Test Suite.

Verifies:
  1. Create and save a document.
  2. Load the saved document and verify its content.
  3. Restart the server and reopen the document (crash/restart recovery).
  4. Two clients editing the same room.
  5. Different rooms remaining strictly isolated.
  6. Presence appearing and disappearing (leave/offline event on disconnect).
  7. Cursor updates being delivered to the correct room.
  8. Client disconnection and reconnection with missed-operation recovery.
  9. Duplicate or out-of-order operations handled idempotently.
 10. Replica convergence after concurrent edits and recovery.
"""
import os
os.environ.setdefault('PYTHONIOENCODING', 'utf-8')

import asyncio
import json
import sys
import time
import shutil
import subprocess
import io

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace', line_buffering=True)
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace', line_buffering=True)

try:
    import websockets
except ImportError:
    print("ERROR: 'websockets' package not found. Install with: pip install websockets")
    sys.exit(2)

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8095
URI = f"ws://127.0.0.1:{PORT}"
TEST_DATA_DIR = os.path.join(os.getcwd(), "data", "test_phase5")
SERVER_EXE = os.path.join(os.getcwd(), "build", "collabpad_server.exe")

PASS = "\033[32mPASS\033[0m"
FAIL = "\033[31mFAIL\033[0m"

failures = 0

def check(label: str, condition: bool, detail: str = ""):
    global failures
    status = PASS if condition else FAIL
    print(f"  [{status}] {label}" + (f" -- {detail}" if detail else ""), flush=True)
    if not condition:
        failures += 1

async def try_recv(ws, timeout=1.5):
    try:
        return await asyncio.wait_for(ws.recv(), timeout=timeout)
    except Exception:
        return None

def start_server_process(data_dir=TEST_DATA_DIR):
    cmd = [SERVER_EXE, str(PORT), data_dir]
    proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(0.8)  # Give Winsock time to bind
    return proc

def stop_server_process(proc):
    if proc:
        proc.kill()
        try:
            proc.wait(timeout=2.0)
        except Exception:
            pass
        time.sleep(0.3)

async def wait_server_ready(max_retries=10):
    for _ in range(max_retries):
        try:
            async with websockets.connect(URI):
                return True
        except Exception:
            await asyncio.sleep(0.3)
    return False

async def run_phase5_tests():
    global failures

    print("\n" + "=" * 60)
    print("  CollabPad Phase 5 — Full Test Suite")
    print(f"  Port: {PORT} | Data: {TEST_DATA_DIR}")
    print("=" * 60 + "\n")

    # Clean up test data directory if exists
    if os.path.exists(TEST_DATA_DIR):
        shutil.rmtree(TEST_DATA_DIR, ignore_errors=True)
    os.makedirs(TEST_DATA_DIR, exist_ok=True)

    server_proc = start_server_process()
    ready = await wait_server_ready()
    if not ready:
        print(f"FATAL: Could not connect to test server at {URI}")
        stop_server_process(server_proc)
        sys.exit(1)

    try:
        # =====================================================================
        # 1. Create and save a document
        # =====================================================================
        print("=== Test 1: Create and save a document ===")
        async with websockets.connect(URI) as clientA:
            # Join doc-save-1
            await clientA.send(json.dumps({"type": "join", "docId": "doc-save-1", "siteId": 1, "name": "Alice"}))
            ack = await try_recv(clientA)
            check("Client joins new document 'doc-save-1'", ack and "joined" in ack)

            # Insert "Hello"
            ops = [
                {"id": {"siteId": 1, "clock": 1}, "afterId": {"siteId": 0, "clock": 0}, "value": "H"},
                {"id": {"siteId": 1, "clock": 2}, "afterId": {"siteId": 1, "clock": 1}, "value": "e"},
                {"id": {"siteId": 1, "clock": 3}, "afterId": {"siteId": 1, "clock": 2}, "value": "l"},
                {"id": {"siteId": 1, "clock": 4}, "afterId": {"siteId": 1, "clock": 3}, "value": "l"},
                {"id": {"siteId": 1, "clock": 5}, "afterId": {"siteId": 1, "clock": 4}, "value": "o"},
            ]
            for op in ops:
                await clientA.send(json.dumps({"type": "op", "docId": "doc-save-1", "siteId": 1, "op": op}))

            # Allow I/O to flush
            await asyncio.sleep(0.3)

            # Verify saved file exists on disk
            saved_file = os.path.join(TEST_DATA_DIR, "doc-save-1.json")
            check("Document file exists on disk", os.path.exists(saved_file))

            with open(saved_file, "r", encoding="utf-8") as f:
                saved_json = json.load(f)
            check("Saved snapshot has stable docId", saved_json.get("docId") == "doc-save-1")
            check("Saved snapshot clock is 5", saved_json.get("clock") == 5)
            check("Saved characters count is 5", len(saved_json.get("characters", [])) == 5)
            chars_text = "".join(c.get("value") for c in saved_json.get("characters", []))
            check("Saved characters form 'Hello'", chars_text == "Hello")

        # =====================================================================
        # 2. Load the saved document and verify its content
        # =====================================================================
        print("\n=== Test 2: Load saved document and verify content ===")
        async with websockets.connect(URI) as clientB:
            await clientB.send(json.dumps({"type": "join", "docId": "doc-save-1", "siteId": 2, "name": "Bob"}))
            ack_b = await try_recv(clientB)
            check("Client B receives joined ack with snapshot", ack_b and "snapshot" in ack_b)
            ack_b_obj = json.loads(ack_b)
            snapshot = ack_b_obj.get("snapshot", {})
            chars = "".join(c.get("value") for c in snapshot.get("characters", []))
            check("Loaded document snapshot matches saved text 'Hello'", chars == "Hello")
            check("Loaded document clock matches 5", snapshot.get("clock") == 5)

        # =====================================================================
        # 3. Restart the server and reopen the document
        # =====================================================================
        print("\n=== Test 3: Restart server and reopen document (persistence recovery) ===")
        stop_server_process(server_proc)
        await asyncio.sleep(0.5)

        # Restart server with same data dir
        server_proc = start_server_process()
        ready = await wait_server_ready()
        check("Server restarted successfully", ready)

        async with websockets.connect(URI) as clientC:
            await clientC.send(json.dumps({"type": "join", "docId": "doc-save-1", "siteId": 3, "name": "Carol"}))
            ack_c = await try_recv(clientC)
            check("Client C connects after restart and receives snapshot", ack_c and "snapshot" in ack_c)
            snap_c = json.loads(ack_c).get("snapshot", {})
            text_c = "".join(c.get("value") for c in snap_c.get("characters", []))
            check("Reopened document after restart preserves content 'Hello'", text_c == "Hello")

            # Append '!' to document
            op_ex = {"id": {"siteId": 3, "clock": 6}, "afterId": {"siteId": 1, "clock": 5}, "value": "!"}
            await clientC.send(json.dumps({"type": "op", "docId": "doc-save-1", "siteId": 3, "op": op_ex}))
            await asyncio.sleep(0.3)

            with open(saved_file, "r", encoding="utf-8") as f:
                updated_json = json.load(f)
            updated_text = "".join(c.get("value") for c in updated_json.get("characters", []))
            check("Updated document after restart saved to disk as 'Hello!'", updated_text == "Hello!")

        # =====================================================================
        # 4. Two clients editing the same room
        # =====================================================================
        print("\n=== Test 4: Two clients editing the same room ===")
        async with websockets.connect(URI) as client1, \
                   websockets.connect(URI) as client2:
            await client1.send(json.dumps({"type": "join", "docId": "room-collab", "siteId": 10, "name": "Dev1"}))
            await try_recv(client1)
            await client2.send(json.dumps({"type": "join", "docId": "room-collab", "siteId": 20, "name": "Dev2"}))
            await try_recv(client2)

            # Dev1 inserts 'X'
            op1 = {"id": {"siteId": 10, "clock": 1}, "afterId": {"siteId": 0, "clock": 0}, "value": "X"}
            await client1.send(json.dumps({"type": "op", "docId": "room-collab", "siteId": 10, "op": op1}))
            recv2 = await try_recv(client2)
            check("Dev2 receives Dev1's edit 'X'", recv2 and json.loads(recv2).get("op", {}).get("value") == "X")

            # Dev2 inserts 'Y' after 'X'
            op2 = {"id": {"siteId": 20, "clock": 2}, "afterId": {"siteId": 10, "clock": 1}, "value": "Y"}
            await client2.send(json.dumps({"type": "op", "docId": "room-collab", "siteId": 20, "op": op2}))
            recv1 = await try_recv(client1)
            check("Dev1 receives Dev2's edit 'Y'", recv1 and json.loads(recv1).get("op", {}).get("value") == "Y")

        # =====================================================================
        # 5. Different rooms remaining isolated
        # =====================================================================
        print("\n=== Test 5: Different rooms remaining isolated ===")
        async with websockets.connect(URI) as c_alpha, \
                   websockets.connect(URI) as c_beta:
            await c_alpha.send(json.dumps({"type": "join", "docId": "room-alpha", "siteId": 31, "name": "Alpha"}))
            await try_recv(c_alpha)
            await c_beta.send(json.dumps({"type": "join", "docId": "room-beta", "siteId": 32, "name": "Beta"}))
            await try_recv(c_beta)

            # Send op in alpha
            op_alpha = {"id": {"siteId": 31, "clock": 1}, "afterId": {"siteId": 0, "clock": 0}, "value": "A"}
            await c_alpha.send(json.dumps({"type": "op", "docId": "room-alpha", "siteId": 31, "op": op_alpha}))

            # Beta receives nothing
            recv_beta = await try_recv(c_beta, timeout=0.8)
            check("Client in 'room-beta' receives nothing from 'room-alpha'", recv_beta is None)

            alpha_file = os.path.join(TEST_DATA_DIR, "room-alpha.json")
            beta_file = os.path.join(TEST_DATA_DIR, "room-beta.json")
            check("room-alpha.json created", os.path.exists(alpha_file))
            check("room-beta.json created", os.path.exists(beta_file))

            with open(alpha_file, "r") as f:
                alpha_text = "".join(c.get("value") for c in json.load(f).get("characters", []))
            with open(beta_file, "r") as f:
                beta_text = "".join(c.get("value") for c in json.load(f).get("characters", []))
            check("room-alpha contains 'A'", alpha_text == "A")
            check("room-beta remains empty ''", beta_text == "")

        # =====================================================================
        # 6. Presence appearing and disappearing correctly
        # =====================================================================
        print("\n=== Test 6: Presence appearing and disappearing correctly ===")
        async with websockets.connect(URI) as watcher:
            await watcher.send(json.dumps({"type": "join", "docId": "presence-room", "siteId": 41, "name": "Watcher"}))
            await try_recv(watcher)

            # Collaborator joins
            actor = await websockets.connect(URI)
            await actor.send(json.dumps({"type": "join", "docId": "presence-room", "siteId": 42, "name": "Actor"}))
            await try_recv(actor)
            await actor.send(json.dumps({"type": "presence", "docId": "presence-room", "siteId": 42, "name": "Actor", "status": "online"}))

            pres_msg = await try_recv(watcher)
            check("Watcher receives online presence for Actor", pres_msg and "Actor" in pres_msg and "presence" in pres_msg)

            # Actor disconnects
            await actor.close()
            leave_msg = await try_recv(watcher, timeout=1.5)
            check("Watcher receives leave/offline event when Actor disconnects",
                  leave_msg is not None and ("offline" in leave_msg or "leave" in leave_msg))

        # =====================================================================
        # 7. Cursor updates being delivered to the correct room
        # =====================================================================
        print("\n=== Test 7: Cursor updates delivered to correct room ===")
        async with websockets.connect(URI) as cursor_a, \
                   websockets.connect(URI) as cursor_b, \
                   websockets.connect(URI) as cursor_c:
            await cursor_a.send(json.dumps({"type": "join", "docId": "cur-room-1", "siteId": 51, "name": "A"}))
            await try_recv(cursor_a)
            await cursor_b.send(json.dumps({"type": "join", "docId": "cur-room-1", "siteId": 52, "name": "B"}))
            await try_recv(cursor_b)
            await cursor_c.send(json.dumps({"type": "join", "docId": "cur-room-2", "siteId": 53, "name": "C"}))
            await try_recv(cursor_c)

            # A sends cursor in cur-room-1
            cursor_data = {
                "type": "cursor",
                "docId": "cur-room-1",
                "siteId": 51,
                "name": "A",
                "color": "#6366f1",
                "cursor": {"line": 3, "col": 14, "index": 45}
            }
            await cursor_a.send(json.dumps(cursor_data))

            # B receives cursor
            msg_b = await try_recv(cursor_b)
            check("Client B in same room receives cursor update", msg_b and "cursor" in msg_b and '"col": 14' in msg_b)

            # C does NOT receive cursor
            msg_c = await try_recv(cursor_c, timeout=0.8)
            check("Client C in different room does NOT receive cursor", msg_c is None)

        # =====================================================================
        # 8. Client disconnection and reconnection with recovery
        # =====================================================================
        print("\n=== Test 8: Client disconnection and reconnection ===")
        # Client 1 types initial text
        async with websockets.connect(URI) as c1:
            await c1.send(json.dumps({"type": "join", "docId": "recovery-room", "siteId": 61, "name": "User1"}))
            await try_recv(c1)

            # C1 inserts "SYNC"
            for i, ch in enumerate("SYNC", start=1):
                parent = {"siteId": 0, "clock": 0} if i == 1 else {"siteId": 61, "clock": i - 1}
                op = {"id": {"siteId": 61, "clock": i}, "afterId": parent, "value": ch}
                await c1.send(json.dumps({"type": "op", "docId": "recovery-room", "siteId": 61, "op": op}))
            await asyncio.sleep(0.3)

        # Client 2 connects, disconnects, then reconnects
        c2 = await websockets.connect(URI)
        await c2.send(json.dumps({"type": "join", "docId": "recovery-room", "siteId": 62, "name": "User2"}))
        ack1 = await try_recv(c2)
        check("User2 connects initially and gets snapshot 'SYNC'", ack1 and "SYNC" in ack1 or ("snapshot" in ack1 and len(json.loads(ack1)["snapshot"]["characters"]) == 4))
        await c2.close()  # Disconnect

        # While User2 is offline, User1 makes more edits: inserts '+'
        async with websockets.connect(URI) as c1:
            await c1.send(json.dumps({"type": "join", "docId": "recovery-room", "siteId": 61, "name": "User1"}))
            await try_recv(c1)
            op_plus = {"id": {"siteId": 61, "clock": 5}, "afterId": {"siteId": 61, "clock": 4}, "value": "+"}
            await c1.send(json.dumps({"type": "op", "docId": "recovery-room", "siteId": 61, "op": op_plus}))
            await asyncio.sleep(0.3)

        # User2 reconnects
        async with websockets.connect(URI) as c2_reconnected:
            await c2_reconnected.send(json.dumps({"type": "join", "docId": "recovery-room", "siteId": 62, "name": "User2"}))
            ack_reconn = await try_recv(c2_reconnected)
            check("User2 reconnects and receives updated snapshot", ack_reconn and "snapshot" in ack_reconn)
            snap = json.loads(ack_reconn).get("snapshot", {})
            reconn_text = "".join(c.get("value") for c in snap.get("characters", []))
            check("Missed edits recovered on reconnection ('SYNC+')", reconn_text == "SYNC+")

        # =====================================================================
        # 9. Duplicate or out-of-order operations handled idempotently
        # =====================================================================
        print("\n=== Test 9: Duplicate and out-of-order operations ===")
        async with websockets.connect(URI) as c_dup:
            await c_dup.send(json.dumps({"type": "join", "docId": "dup-room", "siteId": 71, "name": "DupClient"}))
            await try_recv(c_dup)

            op_base = {"id": {"siteId": 71, "clock": 1}, "afterId": {"siteId": 0, "clock": 0}, "value": "A"}
            # Send op once
            await c_dup.send(json.dumps({"type": "op", "docId": "dup-room", "siteId": 71, "op": op_base}))
            # Resend EXACT duplicate op
            await c_dup.send(json.dumps({"type": "op", "docId": "dup-room", "siteId": 71, "op": op_base}))
            await asyncio.sleep(0.3)

            # Request sync snapshot
            await c_dup.send(json.dumps({"type": "sync_req", "docId": "dup-room"}))
            sync_res = await try_recv(c_dup)
            snap_dup = json.loads(sync_res).get("snapshot", {}) if sync_res else {}
            chars_dup = "".join(c.get("value") for c in snap_dup.get("characters", []))
            check("Duplicate op does not duplicate character ('A' count == 1)", chars_dup == "A")

            # Out-of-order delivery: send child before parent
            # Parent: 'B' after 'A' (clock 2)
            # Child:  'C' after 'B' (clock 3)
            op_child = {"id": {"siteId": 71, "clock": 3}, "afterId": {"siteId": 71, "clock": 2}, "value": "C"}
            op_parent = {"id": {"siteId": 71, "clock": 2}, "afterId": {"siteId": 71, "clock": 1}, "value": "B"}

            # Send child FIRST (parent missing)
            await c_dup.send(json.dumps({"type": "op", "docId": "dup-room", "siteId": 71, "op": op_child}))
            await asyncio.sleep(0.2)
            # Send parent SECOND
            await c_dup.send(json.dumps({"type": "op", "docId": "dup-room", "siteId": 71, "op": op_parent}))
            await asyncio.sleep(0.3)

            await c_dup.send(json.dumps({"type": "sync_req", "docId": "dup-room"}))
            sync_res2 = await try_recv(c_dup)
            snap_ooo = json.loads(sync_res2).get("snapshot", {}) if sync_res2 else {}
            chars_ooo = "".join(c.get("value") for c in snap_ooo.get("characters", []))
            check("Out-of-order ops resolve to correct sequence 'ABC'", chars_ooo == "ABC")

        # =====================================================================
        # 10. Replica convergence after concurrent edits and recovery
        # =====================================================================
        print("\n=== Test 10: Replica convergence after concurrent edits ===")
        async with websockets.connect(URI) as c_conv_a, \
                   websockets.connect(URI) as c_conv_b:
            await c_conv_a.send(json.dumps({"type": "join", "docId": "conv-room", "siteId": 81, "name": "ConvA"}))
            await try_recv(c_conv_a)
            await c_conv_b.send(json.dumps({"type": "join", "docId": "conv-room", "siteId": 82, "name": "ConvB"}))
            await try_recv(c_conv_b)

            # Initial shared character 'P'
            op_p = {"id": {"siteId": 81, "clock": 1}, "afterId": {"siteId": 0, "clock": 0}, "value": "P"}
            await c_conv_a.send(json.dumps({"type": "op", "docId": "conv-room", "siteId": 81, "op": op_p}))
            await try_recv(c_conv_b)  # B receives 'P'

            # Concurrent edits after 'P':
            # ConvA inserts '1' after 'P' with clock 2, site 81
            # ConvB concurrently inserts '2' after 'P' with clock 2, site 82
            op_a1 = {"id": {"siteId": 81, "clock": 2}, "afterId": {"siteId": 81, "clock": 1}, "value": "1"}
            op_b2 = {"id": {"siteId": 82, "clock": 2}, "afterId": {"siteId": 81, "clock": 1}, "value": "2"}

            await c_conv_a.send(json.dumps({"type": "op", "docId": "conv-room", "siteId": 81, "op": op_a1}))
            await c_conv_b.send(json.dumps({"type": "op", "docId": "conv-room", "siteId": 82, "op": op_b2}))

            recv_b_from_a = await try_recv(c_conv_b)
            recv_a_from_b = await try_recv(c_conv_a)
            check("B received A's edit", recv_b_from_a is not None)
            check("A received B's edit", recv_a_from_b is not None)

            # Allow server to persist
            await asyncio.sleep(0.3)

            # Query server snapshot
            await c_conv_a.send(json.dumps({"type": "sync_req", "docId": "conv-room"}))
            res = await try_recv(c_conv_a)
            final_snap = json.loads(res).get("snapshot", {}) if res else {}
            final_text = "".join(c.get("value") for c in final_snap.get("characters", []))

            # In RGA tie-breaking: both have clock 2, site 82 > site 81 -> '2' precedes '1' -> "P21"
            check("Deterministic RGA convergence result is 'P21'", final_text == "P21",
                  f"got: {final_text!r} (expected 'P21' by (clock=2, site=82) > (clock=2, site=81))")

    finally:
        stop_server_process(server_proc)

    print("\n" + "=" * 60)
    if failures == 0:
        print("  ALL 10 PHASE 5 TESTS PASSED [OK]")
    else:
        print(f"  {failures} TEST(S) FAILED [!!]")
    print("=" * 60 + "\n")
    return failures

if __name__ == "__main__":
    result = asyncio.run(run_phase5_tests())
    sys.exit(0 if result == 0 else 1)
