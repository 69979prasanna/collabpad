#include <collabpad/Replica.hpp>
#include <iostream>
#include <vector>

using namespace std;

int main() {
    cout << "========================================\n";
    cout << "       COLLABPAD PHASE 1\n";
    cout << "========================================\n\n";

    cout << "Creating replicas...\n\n";

    collabpad::Replica replicaA(1);
    collabpad::Replica replicaB(2);

    cout << "Replica A: Site " << replicaA.getSiteId() << "\n";
    cout << "Replica B: Site " << replicaB.getSiteId() << "\n\n";

    cout << "Replica A inserts:\n";
    const string textToInsert = "Hello";
    vector<collabpad::InsertOp> operations;

    for (char ch : textToInsert) {
        cout << ch << "\n";
        operations.push_back(replicaA.localInsert(ch));
    }

    cout << "\nReplica A:\n";
    cout << replicaA.getText() << "\n\n";

    cout << "Sending characters to Replica B...\n\n";
    for (const auto& op : operations) {
        replicaB.applyRemoteInsert(op);
    }

    cout << "Replica B:\n";
    cout << replicaB.getText() << "\n\n";

    cout << "----------------------------------------\n";
    cout << "REPLICAS CONVERGED\n";
    cout << "----------------------------------------\n\n";

    cout << "A: " << replicaA.getText() << "\n";
    cout << "B: " << replicaB.getText() << "\n\n";

    cout << "========================================\n";

    return 0;
}
