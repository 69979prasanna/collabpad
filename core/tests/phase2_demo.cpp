#include <collabpad/Operation.hpp>
#include <collabpad/Replica.hpp>
#include <iomanip>
#include <iostream>
#include <vector>

void printDivider(char ch = '=', int length = 50) {
    std::cout << std::string(length, ch) << "\n";
}

void printReplicaState(const std::string& label, const collabpad::Replica& replica) {
    std::cout << label << " [Site " << replica.getSiteId() << "]: \"" 
              << replica.getText() << "\"\n";
}

int main() {
    printDivider();
    std::cout << "       COLLABPAD PHASE 2: RGA CONCURRENT ENGINE\n";
    printDivider();
    std::cout << "\n";

    // -------------------------------------------------------------------------
    // Step 1: Initializing Replicas
    // -------------------------------------------------------------------------
    std::cout << "[Step 1] Initializing Replicas...\n";
    collabpad::Replica replicaA(1);
    collabpad::Replica replicaB(2);

    printReplicaState("Replica A", replicaA);
    printReplicaState("Replica B", replicaB);
    std::cout << "\n";

    // -------------------------------------------------------------------------
    // Step 2: Establish Initial Shared Text ("CAT")
    // -------------------------------------------------------------------------
    std::cout << "[Step 2] Establishing initial shared text: \"CAT\"\n";
    
    // Insert 'C' at beginning (nullopt parent)
    auto opC = replicaA.localInsertAtBeginning('C');
    // Insert 'A' after 'C'
    auto opA = replicaA.localInsertAfter(opC.id, 'A');
    // Insert 'T' after 'A'
    auto opT = replicaA.localInsertAfter(opA.id, 'T');

    // Transfer initial operations to Replica B
    replicaB.applyOperation(opC);
    replicaB.applyOperation(opA);
    replicaB.applyOperation(opT);

    std::cout << "Initial text after sync:\n";
    printReplicaState("  Replica A", replicaA);
    printReplicaState("  Replica B", replicaB);
    std::cout << "\n";

    // Keep reference to character 'A' ID for concurrent insertions
    collabpad::CharId parentAId = opA.id;
    std::cout << "Parent character for concurrent edits: 'A' (Site: " 
              << parentAId.siteId << ", Clock: " << parentAId.clock << ")\n\n";

    // -------------------------------------------------------------------------
    // Step 3: Concurrent Insertions after the Same Parent ('A')
    // -------------------------------------------------------------------------
    std::cout << "[Step 3] Performing CONCURRENT insertions after 'A'...\n";
    
    // Replica A inserts 'R' after 'A' -> Intended word: "CART"
    auto opR = replicaA.localInsertAfter(parentAId, 'R');
    std::cout << "Replica A generates op: Insert '" << opR.value << "' after 'A'\n";
    std::cout << "  CharId: (Site " << opR.id.siteId << ", Clock " << opR.id.clock << ")\n";
    printReplicaState("  Replica A local view", replicaA);

    // Replica B concurrently inserts 'B' after 'A' -> Intended word: "CABT"
    auto opB = replicaB.localInsertAfter(parentAId, 'B');
    std::cout << "Replica B generates op: Insert '" << opB.value << "' after 'A'\n";
    std::cout << "  CharId: (Site " << opB.id.siteId << ", Clock " << opB.id.clock << ")\n";
    printReplicaState("  Replica B local view", replicaB);
    std::cout << "\n";

    // -------------------------------------------------------------------------
    // Step 4: Applying Concurrent Operations in Opposite Arrival Orders
    // -------------------------------------------------------------------------
    std::cout << "[Step 4] Exchanging operations in different arrival orders...\n";
    
    // Arrival order for Replica A: saw opR (local) first, now receives opB (remote)
    std::cout << "--> Replica A receives opB ('" << opB.value << "')...\n";
    replicaA.applyOperation(opB);
    printReplicaState("    Replica A current text", replicaA);

    // Arrival order for Replica B: saw opB (local) first, now receives opR (remote)
    std::cout << "--> Replica B receives opR ('" << opR.value << "')...\n";
    replicaB.applyOperation(opR);
    printReplicaState("    Replica B current text", replicaB);
    std::cout << "\n";

    // -------------------------------------------------------------------------
    // Step 5: Verification of Convergence
    // -------------------------------------------------------------------------
    printDivider('-');
    std::cout << "CONCURRENCY RESOLUTION RESULT\n";
    printDivider('-');
    std::cout << "Replica A text: \"" << replicaA.getText() << "\"\n";
    std::cout << "Replica B text: \"" << replicaB.getText() << "\"\n";

    if (replicaA.getText() == replicaB.getText()) {
        std::cout << "SUCCESS: Both replicas converged to identical text!\n";
        std::cout << "Ordering rule explanation: ('B' ID: site " << opB.id.siteId 
                  << ", clk " << opB.id.clock << ") > ('R' ID: site " << opR.id.siteId 
                  << ", clk " << opR.id.clock << ") -> 'B' precedes 'R'.\n";
    } else {
        std::cerr << "FAILURE: Replicas diverged!\n";
        return 1;
    }
    std::cout << "\n";

    // -------------------------------------------------------------------------
    // Step 6: Handling Missing Parent Dependencies (Buffering Demonstration)
    // -------------------------------------------------------------------------
    std::cout << "[Step 6] Demonstrating Out-Of-Order Delivery & Pending Buffer...\n";
    
    // Replica A inserts '!' after 'T', then '?' after '!'
    auto opExclamation = replicaA.localInsertAfter(opT.id, '!');
    auto opQuestion = replicaA.localInsertAfter(opExclamation.id, '?');
    printReplicaState("Replica A after adding \"!?\"", replicaA);

    // Simulate network reordering: Deliver opQuestion BEFORE opExclamation to Replica B
    std::cout << "\nDelivering opQuestion ('?') to Replica B FIRST (parent '!' has not arrived yet)...\n";
    bool appliedImmediately = replicaB.applyOperation(opQuestion);
    std::cout << "Immediate apply result: " << (appliedImmediately ? "true" : "false (buffered)") << "\n";
    std::cout << "Replica B pending buffer count: " << replicaB.getPendingCount() << "\n";
    printReplicaState("Replica B text while waiting", replicaB);

    // Now deliver opExclamation ('!') to Replica B
    std::cout << "\nNow delivering opExclamation ('!') to Replica B...\n";
    replicaB.applyOperation(opExclamation);
    std::cout << "Replica B pending buffer count after parent arrived: " 
              << replicaB.getPendingCount() << "\n";
    printReplicaState("Replica B text after pending resolved", replicaB);
    std::cout << "\n";

    // Final Convergence Check
    printDivider('-');
    std::cout << "FINAL SYSTEM CONVERGENCE CHECK\n";
    printDivider('-');
    std::cout << "Replica A: \"" << replicaA.getText() << "\"\n";
    std::cout << "Replica B: \"" << replicaB.getText() << "\"\n";

    if (replicaA.getText() == replicaB.getText()) {
        std::cout << "SUCCESS: Both replicas perfectly in sync: \"" << replicaA.getText() << "\"\n";
    } else {
        std::cerr << "FAILURE: Inconsistent state!\n";
        return 1;
    }

    printDivider();
    return 0;
}
