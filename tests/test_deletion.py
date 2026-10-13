#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# PYTHONIOENCODING=utf-8
"""
tests/test_deletion.py
-----------------------
Phase 6 End-to-End WebSocket Integration & Deletion Verification Suite.

Verifies:
  1. Two clients connect to a room: Client 1 types "HELLO", Client 2 syncs "HELLO".
  2. Client 1 deletes "LL". Client 2 receives deletes in real-time without echoing.
     Both clients observe visible text "HEO".
  3. Client 2 inserts "LP" concurrently after 'E'. Both replicas converge to "HELPO".
  4. Out-of-order delete handling: delete operation arrives before insert; buffered
     and tombstoned automatically upon insert arrival.
  5. Duplicate deletion idempotency over WebSocket.
  6. Persistence & restart recovery: Server is killed and restarted; room snapshot
     restores tombstones with isDeleted=true, and deleted characters never ghost back.
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

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8096
URI = f"ws://127.0.0.1:{PORT}"
TEST_DATA_DIR = os.path.join(os.getcwd(), "data", "test_deletion")
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

async def try_recv(ws, timeout=2.0):
    try:
        return await asyncio.wait_for(ws.recv(), timeout=timeout)
    except Exception:
        return None

def start_server_process(data_dir=TEST_DATA_DIR):
    cmd = [SERVER_EXE, str(PORT), data_dir]
    proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(0.8)
    return proc

def stop_server_process(proc):
    if proc:
        proc.kill()
        try:
            proc.wait(timeout=2.0)
        except Exception:
            pass
        time.sleep(0.3)

async def wait_server_ready(max_retries=15):
    for _ in range(max_retries):
        try:
            async with websockets.connect(URI):
                return True
        except Exception:
            await asyncio.sleep(0.3)
    return False

def get_visible_text(characters):
    return "".join(c.get("value", "") for c in characters if not c.get("deleted", False))

async def run_deletion_tests():
    global failures

    print("\n" + "=" * 60)
    print("  CollabPad Phase 6 — End-to-End Deletion & Recovery Suite")
    print(f"  Port: {PORT} | Data: {TEST_DATA_DIR}")
    print("=" * 60 + "\n")

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
        # Case 1: Two clients connect and Client 1 inserts "HELLO"
        # =====================================================================
        print("=== Test 1: Insert 'HELLO' and synchronize across 2 clients ===")
        async with websockets.connect(URI) as client1, websockets.connect(URI) as client2:
            # Both join room "doc-del-1"
            await client1.send(json.dumps({"type": "join", "docId": "doc-del-1", "siteId": 1, "name": "Alice"}))
            ack1 = await try_recv(client1)
            check("Client 1 joins 'doc-del-1'", ack1 and "joined" in ack1)

            await client2.send(json.dumps({"type": "join", "docId": "doc-del-1", "siteId": 2, "name": "Bob"}))
            ack2 = await try_recv(client2)
            check("Client 2 joins 'doc-del-1'", ack2 and "joined" in ack2)

            # Client 1 inserts "HELLO"
            ops = [
                {"id": {"siteId": 1, "clock": 1}, "afterId": {"siteId": 0, "clock": 0}, "value": "H"},
                {"id": {"siteId": 1, "clock": 2}, "afterId": {"siteId": 1, "clock": 1}, "value": "E"},
                {"id": {"siteId": 1, "clock": 3}, "afterId": {"siteId": 1, "clock": 2}, "value": "L"},
                {"id": {"siteId": 1, "clock": 4}, "afterId": {"siteId": 1, "clock": 3}, "value": "L"},
                {"id": {"siteId": 1, "clock": 5}, "afterId": {"siteId": 1, "clock": 4}, "value": "O"},
            ]
            for op in ops:
                await client1.send(json.dumps({"type": "op", "docId": "doc-del-1", "siteId": 1, "op": op}))

            # Client 2 should receive all 5 ops
            received_ops = []
            for _ in range(5):
                msg = await try_recv(client2)
                if msg:
                    parsed = json.loads(msg)
                    if parsed.get("type") == "op":
                        received_ops.append(parsed.get("op", {}).get("value"))
            check("Client 2 receives all 5 character inserts in real time", "".join(received_ops) == "HELLO")

        # =====================================================================
        # Case 2: Client 1 deletes "LL", Client 2 receives deletes in real time
        # =====================================================================
        print("\n=== Test 2: Client 1 deletes 'LL' -> Both observe 'HEO' ===")
        async with websockets.connect(URI) as client1, websockets.connect(URI) as client2:
            await client1.send(json.dumps({"type": "join", "docId": "doc-del-1", "siteId": 1, "name": "Alice"}))
            ack1 = json.loads(await try_recv(client1))
            await client2.send(json.dumps({"type": "join", "docId": "doc-del-1", "siteId": 2, "name": "Bob"}))
            ack2 = json.loads(await try_recv(client2))

            snap = ack1.get("snapshot", {})
            check("Initial snapshot has 5 characters", len(snap.get("characters", [])) == 5)
            check("Initial snapshot visible text is 'HELLO'", get_visible_text(snap.get("characters", [])) == "HELLO")

            # Client 1 deletes second 'L' (1:4) and first 'L' (1:3)
            del_op1 = {
                "type": "delete",
                "siteId": 1,
                "clock": 6,
                "targetId": {"siteId": 1, "clock": 4}
            }
            del_op2 = {
                "type": "delete",
                "siteId": 1,
                "clock": 7,
                "targetId": {"siteId": 1, "clock": 3}
            }
            await client1.send(json.dumps({"type": "op", "docId": "doc-del-1", "siteId": 1, "op": del_op1}))
            await client1.send(json.dumps({"type": "op", "docId": "doc-del-1", "siteId": 1, "op": del_op2}))

            # Client 2 receives both delete ops
            del_received = []
            for _ in range(2):
                msg = await try_recv(client2)
                if msg:
                    parsed = json.loads(msg)
                    if parsed.get("type") == "op" and parsed.get("op", {}).get("type") == "delete":
                        del_received.append(parsed.get("op"))
            check("Client 2 receives both deletion operations in real time", len(del_received) == 2)
            check("Deletion targets match (1:4) and (1:3)",
                  del_received[0].get("targetId", {}).get("clock") == 4 and
                  del_received[1].get("targetId", {}).get("clock") == 3)

        # =====================================================================
        # Case 3: Concurrent insertion after tombstoned character
        # =====================================================================
        print("\n=== Test 3: Insert 'LP' after 'E' -> Both converge to 'HELPO' ===")
        async with websockets.connect(URI) as client2:
            await client2.send(json.dumps({"type": "join", "docId": "doc-del-1", "siteId": 2, "name": "Bob"}))
            ack2 = json.loads(await try_recv(client2))
            snap2 = ack2.get("snapshot", {})
            check("Snapshot text after deletions is 'HEO'", get_visible_text(snap2.get("characters", [])) == "HEO")

            # Client 2 inserts 'L' after 'E' (1:2), then 'P' after new 'L'
            ins_l = {"id": {"siteId": 2, "clock": 8}, "afterId": {"siteId": 1, "clock": 2}, "value": "L"}
            ins_p = {"id": {"siteId": 2, "clock": 9}, "afterId": {"siteId": 2, "clock": 8}, "value": "P"}
            await client2.send(json.dumps({"type": "op", "docId": "doc-del-1", "siteId": 2, "op": ins_l}))
            await client2.send(json.dumps({"type": "op", "docId": "doc-del-1", "siteId": 2, "op": ins_p}))
            await asyncio.sleep(0.3)

        # Verify room state
        async with websockets.connect(URI) as client3:
            await client3.send(json.dumps({"type": "join", "docId": "doc-del-1", "siteId": 3, "name": "Carol"}))
            ack3 = json.loads(await try_recv(client3))
            snap3 = ack3.get("snapshot", {})
            vis_text = get_visible_text(snap3.get("characters", []))
            check("Document converges to 'HELPO'", vis_text == "HELPO", f"Got: '{vis_text}'")

        # =====================================================================
        # Case 4: Out-of-order delete arriving before insert
        # =====================================================================
        print("\n=== Test 4: Out-of-order delete arriving before character insert ===")
        async with websockets.connect(URI) as client4:
            await client4.send(json.dumps({"type": "join", "docId": "doc-ooo", "siteId": 4, "name": "Dave"}))
            await try_recv(client4)

            # Send delete for CharId (4, 10) before inserting it
            ooo_del = {
                "type": "delete",
                "siteId": 4,
                "clock": 12,
                "targetId": {"siteId": 4, "clock": 10}
            }
            await client4.send(json.dumps({"type": "op", "docId": "doc-ooo", "siteId": 4, "op": ooo_del}))
            await asyncio.sleep(0.2)

            # Now send the insertion for CharId (4, 10)
            ooo_ins = {"id": {"siteId": 4, "clock": 10}, "afterId": {"siteId": 0, "clock": 0}, "value": "Z"}
            await client4.send(json.dumps({"type": "op", "docId": "doc-ooo", "siteId": 4, "op": ooo_ins}))
            await asyncio.sleep(0.3)

        async with websockets.connect(URI) as client5:
            await client5.send(json.dumps({"type": "join", "docId": "doc-ooo", "siteId": 5, "name": "Eve"}))
            ack5 = json.loads(await try_recv(client5))
            snap5 = ack5.get("snapshot", {})
            check("Out-of-order character was tombstoned upon arrival (visible text is empty)",
                  get_visible_text(snap5.get("characters", [])) == "")
            check("Underlying character node exists as tombstone",
                  len(snap5.get("characters", [])) == 1 and snap5["characters"][0].get("deleted") == True)

        # =====================================================================
        # Case 5: Duplicate deletion idempotency
        # =====================================================================
        print("\n=== Test 5: Duplicate deletion idempotency ===")
        async with websockets.connect(URI) as client6:
            await client6.send(json.dumps({"type": "join", "docId": "doc-del-1", "siteId": 6, "name": "Frank"}))
            await try_recv(client6)

            # Send duplicate delete for (1:4)
            await client6.send(json.dumps({"type": "op", "docId": "doc-del-1", "siteId": 6, "op": del_op1}))
            await asyncio.sleep(0.2)

            ack6 = json.loads(await try_recv(client6) or "{}")
            # Document text should remain "HELPO"
        async with websockets.connect(URI) as client7:
            await client7.send(json.dumps({"type": "join", "docId": "doc-del-1", "siteId": 7, "name": "Grace"}))
            ack7 = json.loads(await try_recv(client7))
            snap7 = ack7.get("snapshot", {})
            check("Duplicate delete did not affect text ('HELPO' preserved)",
                  get_visible_text(snap7.get("characters", [])) == "HELPO")

        # =====================================================================
        # Case 6: Server crash & restart recovery (no deleted character ghosting)
        # =====================================================================
        print("\n=== Test 6: Server restart recovery — deleted characters do NOT ghost back ===")
        stop_server_process(server_proc)
        await asyncio.sleep(0.5)

        # Restart server with same data dir
        server_proc = start_server_process()
        ready = await wait_server_ready()
        check("Server restarted successfully", ready)

        async with websockets.connect(URI) as clientRestored:
            await clientRestored.send(json.dumps({"type": "join", "docId": "doc-del-1", "siteId": 8, "name": "Hank"}))
            ack_restored = await try_recv(clientRestored)
            check("Client receives snapshot after server restart", ack_restored and "snapshot" in ack_restored)
            snap_restored = json.loads(ack_restored).get("snapshot", {})
            vis_text_after_restart = get_visible_text(snap_restored.get("characters", []))
            check("Visible text after restart is exactly 'HELPO'", vis_text_after_restart == "HELPO",
                  f"Got: '{vis_text_after_restart}'")
            # Verify tombstoned characters 'L' and 'L' are still present but marked deleted=true
            tombstones = [c for c in snap_restored.get("characters", []) if c.get("deleted") == True]
            check("Tombstones are retained on disk with deleted=True", len(tombstones) == 2)
            check("No ghost characters reappeared after restart", "HELLO" not in vis_text_after_restart)

    finally:
        stop_server_process(server_proc)

    print("\n" + "=" * 60)
    if failures == 0:
        print(f"  {PASS} ALL PHASE 6 E2E INTEGRATION TESTS PASSED (0 FAILURES)")
    else:
        print(f"  {FAIL} {failures} TESTS FAILED")
    print("=" * 60 + "\n")
    sys.exit(0 if failures == 0 else 1)

if __name__ == "__main__":
    asyncio.run(run_deletion_tests())
