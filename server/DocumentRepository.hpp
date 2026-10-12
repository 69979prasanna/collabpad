#pragma once

#include <collabpad/DocumentSnapshot.hpp>
#include <string>
#include <vector>

namespace collabpad {

/**
 * @brief Abstract interface for document persistence repositories.
 * Defines standard operations for creating, saving, loading, and listing documents.
 */
class DocumentRepository {
public:
    virtual ~DocumentRepository() = default;

    /**
     * @brief Persists a document snapshot to durable storage.
     * @param snapshot The snapshot containing docId, clock, characters, and metadata.
     * @return true on success, false if an error occurred.
     */
    virtual bool save(const DocumentSnapshot& snapshot) = 0;

    /**
     * @brief Loads a document snapshot by its ID.
     * @param docId The stable identifier of the document.
     * @param outSnapshot Populated with the loaded snapshot if successful.
     * @return true if the document was found and successfully parsed, false otherwise.
     */
    virtual bool load(const std::string& docId, DocumentSnapshot& outSnapshot) = 0;

    /**
     * @brief Checks whether a document exists in the repository.
     */
    virtual bool exists(const std::string& docId) = 0;

    /**
     * @brief Lists all document IDs available in the repository.
     */
    virtual std::vector<std::string> listDocuments() = 0;
};

} // namespace collabpad
