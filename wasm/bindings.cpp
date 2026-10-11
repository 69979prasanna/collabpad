// wasm/bindings.cpp
// WebAssembly bindings for CollabPad CRDT Engine using Emscripten Embind.
#include <collabpad/CharId.hpp>
#include <collabpad/Operation.hpp>
#include <collabpad/Replica.hpp>
#include <collabpad/Sequence.hpp>

#include <emscripten/bind.h>
#include <string>

using namespace std;
using namespace emscripten;

// Helper wrappers so JS can query character IDs and indices conveniently
collabpad::CharId replicaGetIdAt(const collabpad::Replica& replica, size_t index) {
    return replica.getSequence().getIdAt(index);
}

int replicaFindIndex(const collabpad::Replica& replica, const collabpad::CharId& id) {
    return replica.getSequence().findIndex(id);
}

size_t replicaGetSize(const collabpad::Replica& replica) {
    return replica.getSequence().size();
}

EMSCRIPTEN_BINDINGS(collabpad_module) {
    // CharId
    value_object<collabpad::CharId>("CharId")
        .field("siteId", &collabpad::CharId::siteId)
        .field("clock",  &collabpad::CharId::clock);

    // InsertionOp
    value_object<collabpad::InsertionOp>("InsertionOp")
        .field("id",      &collabpad::InsertionOp::id)
        .field("afterId", &collabpad::InsertionOp::afterId)
        .field("value",   &collabpad::InsertionOp::value);

    // Replica
    class_<collabpad::Replica>("Replica")
        .constructor<uint32_t>()
        .function("localInsertAfter", &collabpad::Replica::localInsertAfter)
        .function("localInsertAtBeginning", &collabpad::Replica::localInsertAtBeginning)
        .function("applyOperation", &collabpad::Replica::applyOperation)
        .function("getText", &collabpad::Replica::getText)
        .function("getSiteId", &collabpad::Replica::getSiteId)
        .function("hasPendingOperations", &collabpad::Replica::hasPendingOperations)
        .function("getPendingCount", &collabpad::Replica::getPendingCount)
        .function("getIdAt", &replicaGetIdAt)
        .function("findIndex", &replicaFindIndex)
        .function("size", &replicaGetSize);
}
