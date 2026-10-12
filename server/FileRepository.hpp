#pragma once

#include "DocumentRepository.hpp"
#include <string>
#include <vector>

namespace collabpad {

/**
 * @brief File-based implementation of DocumentRepository.
 * Persists documents as formatted JSON snapshots in a dedicated storage directory.
 * Uses atomic temporary-file writes to prevent corrupted or partial file writes.
 */
class FileRepository : public DocumentRepository {
public:
    explicit FileRepository(const std::string& baseDir = "data/documents");
    ~FileRepository() override = default;

    bool save(const DocumentSnapshot& snapshot) override;
    bool load(const std::string& docId, DocumentSnapshot& outSnapshot) override;
    bool exists(const std::string& docId) override;
    std::vector<std::string> listDocuments() override;

    const std::string& getBaseDir() const { return baseDir_; }

private:
    bool ensureDirectoryExists(const std::string& dirPath);
    std::string getDocFilePath(const std::string& docId) const;
    std::string getTempFilePath(const std::string& docId) const;
    std::string sanitizeDocId(const std::string& docId) const;

    std::string baseDir_;
};

} // namespace collabpad
