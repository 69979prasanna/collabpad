#include <collabpad/Replica.hpp>
#include <iostream>
#include <vector>

int main() {
    std::cout << "========================================\n";
    std::cout << "       COLLABPAD PHASE 1\n";
    std::cout << "========================================\n\n";

    std::cout << "Creating replicas...\n\n";

    collabpad::Replica replicaA(1);
    collabpad::Replica replicaB(2);

    std::cout << "Replica A: Site " << replicaA.getSiteId() << "\n";
    std::cout << "Replica B: Site " << replicaB.getSiteId() << "\n\n";

    std::cout << "Replica A inserts:\n";
    const std::string textToInsert = "Hello";
    std::vector<collabpad::InsertOp> operations;

    for (char ch : textToInsert) {
        std::cout << ch << "\n";
        operations.push_back(replicaA.localInsert(ch));
    }

    std::cout << "\nReplica A:\n";
    std::cout << replicaA.getText() << "\n\n";

    std::cout << "Sending characters to Replica B...\n\n";
    for (const auto& op : operations) {
        replicaB.applyRemoteInsert(op);
    }

    std::cout << "Replica B:\n";
    std::cout << replicaB.getText() << "\n\n";

    std::cout << "----------------------------------------\n";
    std::cout << "REPLICAS CONVERGED\n";
    std::cout << "----------------------------------------\n\n";

    std::cout << "A: " << replicaA.getText() << "\n";
    std::cout << "B: " << replicaB.getText() << "\n\n";

    std::cout << "========================================\n";

    return 0;
}
