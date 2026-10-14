#include <iostream>
#include <cassert>
#include <string>
#include "collabpad/CharId.hpp"
#include "collabpad/Operation.hpp"
#include "collabpad/Sequence.hpp"
#include "collabpad/Replica.hpp"
#include "collabpad/DocumentSnapshot.hpp"

using namespace std;
using namespace collabpad;

void printHeader(const string& title) {
    cout << "\n==================================================\n";
    cout << "       " << title << "\n";
    cout << "==================================================\n";
}

int main() {
    printHeader("COLLABPAD PHASE 6: RGA DELETION & TOMBSTONES");

    // -------------------------------------------------------------------------
    // Test Case 1: Sequential Insertion and Deletion with Tombstones
    // -------------------------------------------------------------------------
    cout << "\n[Case 1] Sequential Insertion and Deletion with Tombstones...\n";
    Replica rA(1);
    auto opH = rA.localInsertAfter(CharId::root(), 'H');
    auto opE = rA.localInsertAfter(opH.id, 'E');
    auto opL1 = rA.localInsertAfter(opE.id, 'L');
    auto opL2 = rA.localInsertAfter(opL1.id, 'L');
    auto opO = rA.localInsertAfter(opL2.id, 'O');

    cout << "  Initial text: \"" << rA.getText() << "\"\n";
    cout << "  Visible size: " << rA.visibleSize() << " | Raw node count: " << rA.getSequence().size() << "\n";
    assert(rA.getText() == "HELLO");
    assert(rA.visibleSize() == 5);
    assert(rA.getSequence().size() == 5);

    // Delete the first 'L' (visual index 2)
    DeletionOp delL1 = rA.localDeleteAt(2);
    cout << "  Deleted character at visual index 2 (target CharId: " 
         << delL1.targetId.siteId << ":" << delL1.targetId.clock << ")\n";
    cout << "  Text after deletion: \"" << rA.getText() << "\"\n";
    cout << "  Visible size: " << rA.visibleSize() << " | Raw node count: " << rA.getSequence().size() << "\n";
    assert(rA.getText() == "HELO");
    assert(rA.visibleSize() == 4);
    assert(rA.getSequence().size() == 5); // Tombstone retained in memory
    assert(rA.getSequence().isTombstone(opL1.id));
    cout << "  [PASS] Case 1: Sequential insertion and deletion verified.\n";

    // -------------------------------------------------------------------------
    // Test Case 2: Concurrent Insert + Delete Convergence
    // -------------------------------------------------------------------------
    cout << "\n[Case 2] Concurrent Insert and Delete Resolution...\n";
    Replica repA(1);
    Replica repB(2);

    // Both establish common baseline "CAT"
    auto cC = repA.localInsertAfter(CharId::root(), 'C');
    auto cA = repA.localInsertAfter(cC.id, 'A');
    auto cT = repA.localInsertAfter(cA.id, 'T');

    repB.applyOperation(cC);
    repB.applyOperation(cA);
    repB.applyOperation(cT);
    assert(repA.getText() == "CAT");
    assert(repB.getText() == "CAT");
    cout << "  Shared baseline: \"" << repA.getText() << "\"\n";

    // Replica A deletes 'A'
    DeletionOp delA = repA.localDelete(cA.id);
    cout << "  Replica A deletes 'A' -> local text: \"" << repA.getText() << "\"\n";
    assert(repA.getText() == "CT");

    // Concurrently, Replica B inserts 'R' after 'A' (intending "CART")
    InsertionOp insR = repB.localInsertAfter(cA.id, 'R');
    cout << "  Replica B inserts 'R' after 'A' -> local text: \"" << repB.getText() << "\"\n";
    assert(repB.getText() == "CART");

    // Cross-exchange operations
    cout << "  Exchanging operations across replicas...\n";
    repA.applyOperation(insR);
    repB.applyDelete(delA);

    cout << "  Replica A converged text: \"" << repA.getText() << "\"\n";
    cout << "  Replica B converged text: \"" << repB.getText() << "\"\n";
    assert(repA.getText() == "CRT");
    assert(repB.getText() == "CRT");
    assert(repA.getText() == repB.getText());
    cout << "  [PASS] Case 2: Concurrent insert and delete converged deterministically to \"CRT\".\n";

    // -------------------------------------------------------------------------
    // Test Case 3: Duplicate Deletion Idempotency
    // -------------------------------------------------------------------------
    cout << "\n[Case 3] Duplicate Deletion Idempotency...\n";
    bool dup1 = repA.applyDelete(delA);
    cout << "  Re-applying deletion of 'A' to Replica A: returned " << (dup1 ? "true" : "false (idempotent)") << "\n";
    assert(dup1 == false);
    assert(repA.getText() == "CRT");

    bool dup2 = repB.applyDelete(delA);
    cout << "  Re-applying deletion of 'A' to Replica B: returned " << (dup2 ? "true" : "false (idempotent)") << "\n";
    assert(dup2 == false);
    assert(repB.getText() == "CRT");
    cout << "  [PASS] Case 3: Duplicate deletion operations are strictly idempotent.\n";

    // -------------------------------------------------------------------------
    // Test Case 4: Out-Of-Order Delivery (Delete Arrives Before Insert)
    // -------------------------------------------------------------------------
    cout << "\n[Case 4] Out-of-Order Delivery (Delete Arriving Before Insert)...\n";
    Replica repC(3);
    CharId futureCharId(9, 100);
    DeletionOp earlyDelete(futureCharId, 9, 105);

    cout << "  Delivering DeletionOp for unarrived CharId (9:100)...\n";
    bool earlyApplied = repC.applyDelete(earlyDelete);
    assert(earlyApplied == false);
    assert(repC.hasPendingDeletions());
    assert(repC.getPendingDeletionCount() == 1);
    cout << "  Buffered in pendingDeletions: count = " << repC.getPendingDeletionCount() << "\n";

    // Now the insertion arrives
    InsertionOp lateInsert(futureCharId, CharId::root(), 'X');
    cout << "  Now delivering late InsertionOp for CharId (9:100)...\n";
    bool insResult = repC.applyOperation(lateInsert);
    assert(insResult == true);
    assert(repC.getText() == ""); // Immediately tombstoned!
    assert(repC.visibleSize() == 0);
    assert(repC.getSequence().size() == 1);
    assert(repC.getSequence().isTombstone(futureCharId));
    assert(!repC.hasPendingDeletions());
    cout << "  Pending deletion automatically matched and tombstone applied upon arrival!\n";
    cout << "  [PASS] Case 4: Out-of-order delete handling verified.\n";

    // -------------------------------------------------------------------------
    // Test Case 5: Document Snapshot Serialization & Restart Recovery
    // -------------------------------------------------------------------------
    cout << "\n[Case 5] Document Snapshot Serialization & Restart Recovery...\n";
    // Prepare a pending deletion in repA for persistence check
    DeletionOp unarrivedDel(CharId{8, 88}, 8, 90);
    repA.applyDelete(unarrivedDel);

    DocumentSnapshot snap = repA.createSnapshot("demo-room", "Demo Room");
    string json = snap.toJson();
    cout << "  Serialized Snapshot JSON preview: " << json.substr(0, 110) << "...\n";
    assert(json.find("\"deleted\":true") != string::npos);
    assert(json.find("\"pendingDeletions\"") != string::npos);

    // Simulate server crash & restart by restoring into a fresh replica
    DocumentSnapshot restoredSnap;
    bool parseOk = DocumentSnapshot::fromJson(json, restoredSnap);
    assert(parseOk);

    Replica repServerRestart(0);
    repServerRestart.loadSnapshot(restoredSnap);

    cout << "  Restored text after restart: \"" << repServerRestart.getText() << "\"\n";
    cout << "  Restored visible size: " << repServerRestart.visibleSize() 
         << " | Raw node count: " << repServerRestart.getSequence().size() << "\n";
    cout << "  Restored pending deletions count: " << repServerRestart.getPendingDeletionCount() << "\n";
    assert(repServerRestart.getText() == "CRT");
    assert(repServerRestart.visibleSize() == 3);
    assert(repServerRestart.getSequence().size() == 4); // C, A (tombstone), R, T
    assert(repServerRestart.getSequence().isTombstone(cA.id));
    assert(repServerRestart.hasPendingDeletions());
    assert(repServerRestart.getPendingDeletionCount() == 1);
    cout << "  [PASS] Case 5: Tombstones and pending deletions survive snapshot serialization.\n";

    // -------------------------------------------------------------------------
    // Test Case 6: Backward Compatibility with Legacy Snapshots
    // -------------------------------------------------------------------------
    cout << "\n[Case 6] Backward Compatibility with Legacy Snapshots (No Deletion Fields)...\n";
    string legacyJson = "{\"docId\":\"legacy-doc\",\"title\":\"Legacy Title\",\"clock\":3,"
                        "\"characters\":["
                        "{\"id\":{\"siteId\":1,\"clock\":1},\"afterId\":{\"siteId\":0,\"clock\":0},\"value\":\"L\"},"
                        "{\"id\":{\"siteId\":1,\"clock\":2},\"afterId\":{\"siteId\":1,\"clock\":1},\"value\":\"E\"},"
                        "{\"id\":{\"siteId\":1,\"clock\":3},\"afterId\":{\"siteId\":1,\"clock\":2},\"value\":\"G\"}"
                        "]}";
    DocumentSnapshot legacySnap;
    bool legacyOk = DocumentSnapshot::fromJson(legacyJson, legacySnap);
    assert(legacyOk);
    assert(legacySnap.characters.size() == 3);
    assert(!legacySnap.characters[0].isDeleted);
    assert(!legacySnap.characters[1].isDeleted);
    assert(!legacySnap.characters[2].isDeleted);

    Replica legacyReplica(2);
    legacyReplica.loadSnapshot(legacySnap);
    assert(legacyReplica.getText() == "LEG");
    assert(legacyReplica.visibleSize() == 3);
    assert(legacyReplica.getSequence().size() == 3);
    assert(!legacyReplica.hasPendingDeletions());
    assert(legacyReplica.getPendingDeletionCount() == 0);

    // Deletion on restored legacy replica should work seamlessly
    auto delE = legacyReplica.localDeleteAt(1);
    assert(legacyReplica.getText() == "LG");
    assert(legacyReplica.visibleSize() == 2);
    assert(legacyReplica.getSequence().size() == 3);
    assert(legacyReplica.getSequence().isTombstone(delE.targetId));
    cout << "  [PASS] Case 6: Legacy snapshots without deletion metadata load with characters visible.\n";

    printHeader("ALL PHASE 6 C++ VERIFICATION CASES PASSED (0 ERRORS)");
    return 0;
}
