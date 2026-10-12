#include "FileRepository.hpp"
#include <windows.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cctype>

using namespace std;

namespace collabpad {

FileRepository::FileRepository(const string& baseDir)
    : baseDir_(baseDir) {
    if (!ensureDirectoryExists(baseDir_)) {
        cerr << "[FileRepository] WARNING: Could not initialize directory: " << baseDir_ << "\n";
    } else {
        cout << "[FileRepository] Initialized document repository at \"" << baseDir_ << "\"\n";
    }
}

string FileRepository::sanitizeDocId(const string& docId) const {
    string clean;
    for (char c : docId) {
        if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_') {
            clean.push_back(c);
        } else {
            clean.push_back('_');
        }
    }
    return clean.empty() ? "unnamed" : clean;
}

string FileRepository::getDocFilePath(const string& docId) const {
    return baseDir_ + "/" + sanitizeDocId(docId) + ".json";
}

string FileRepository::getTempFilePath(const string& docId) const {
    return baseDir_ + "/" + sanitizeDocId(docId) + ".tmp";
}

bool FileRepository::ensureDirectoryExists(const string& dirPath) {
    if (dirPath.empty()) return true;

    // Split path into parts and create progressively
    string current;
    for (size_t i = 0; i < dirPath.size(); ++i) {
        char c = dirPath[i];
        if (c == '/' || c == '\\') {
            if (!current.empty()) {
                CreateDirectoryA(current.c_str(), NULL);
            }
        }
        current.push_back(c);
    }
    if (!current.empty()) {
        CreateDirectoryA(current.c_str(), NULL);
    }

    DWORD attrs = GetFileAttributesA(dirPath.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY));
}

bool FileRepository::save(const DocumentSnapshot& snapshot) {
    if (snapshot.docId.empty()) {
        cerr << "[FileRepository] ERROR: Cannot save document with empty docId.\n";
        return false;
    }

    if (!ensureDirectoryExists(baseDir_)) {
        cerr << "[FileRepository] ERROR: Base directory does not exist and could not be created: "
             << baseDir_ << "\n";
        return false;
    }

    string tmpPath = getTempFilePath(snapshot.docId);
    string targetPath = getDocFilePath(snapshot.docId);

    // 1. Write to temporary file
    ofstream outFile(tmpPath.c_str(), ios::out | ios::trunc | ios::binary);
    if (!outFile.is_open()) {
        cerr << "[FileRepository] ERROR: Failed to open temporary file for writing: "
             << tmpPath << "\n";
        return false;
    }

    string json = snapshot.toJson();
    outFile.write(json.data(), json.size());
    outFile.flush();

    if (!outFile.good()) {
        cerr << "[FileRepository] ERROR: Stream write failure while saving document \""
             << snapshot.docId << "\" to " << tmpPath << "\n";
        outFile.close();
        remove(tmpPath.c_str());
        return false;
    }
    outFile.close();

    // 2. Atomically move/replace temporary file with target file
    BOOL moved = MoveFileExA(tmpPath.c_str(), targetPath.c_str(),
                             MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!moved) {
        // Fallback for filesystems that do not support atomic MoveFileEx replace
        remove(targetPath.c_str());
        if (rename(tmpPath.c_str(), targetPath.c_str()) != 0) {
            DWORD err = GetLastError();
            cerr << "[FileRepository] ERROR: Could not rename " << tmpPath
                 << " to " << targetPath << " (WinError: " << err << ")\n";
            remove(tmpPath.c_str());
            return false;
        }
    }

    cout << "[FileRepository] Saved document \"" << snapshot.docId
         << "\" (" << snapshot.characters.size() << " chars, clock "
         << snapshot.clock << ") to " << targetPath << "\n";
    return true;
}

bool FileRepository::load(const string& docId, DocumentSnapshot& outSnapshot) {
    if (docId.empty()) {
        return false;
    }

    string targetPath = getDocFilePath(docId);
    ifstream inFile(targetPath.c_str(), ios::in | ios::binary);
    if (!inFile.is_open()) {
        return false;
    }

    stringstream buffer;
    buffer << inFile.rdbuf();
    inFile.close();

    string content = buffer.str();
    if (content.empty()) {
        cerr << "[FileRepository] WARNING: Document file is empty: " << targetPath << "\n";
        return false;
    }

    if (!DocumentSnapshot::fromJson(content, outSnapshot)) {
        cerr << "[FileRepository] ERROR: Corrupted or invalid JSON in " << targetPath << "\n";
        return false;
    }

    cout << "[FileRepository] Loaded document \"" << docId
         << "\" (" << outSnapshot.characters.size() << " chars, clock "
         << outSnapshot.clock << ") from " << targetPath << "\n";
    return true;
}

bool FileRepository::exists(const string& docId) {
    if (docId.empty()) return false;
    string targetPath = getDocFilePath(docId);
    DWORD attrs = GetFileAttributesA(targetPath.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY));
}

vector<string> FileRepository::listDocuments() {
    vector<string> docs;
    string searchPattern = baseDir_ + "/*.json";

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPattern.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return docs;
    }

    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            string filename = findData.cFileName;
            if (filename.size() > 5 && filename.substr(filename.size() - 5) == ".json") {
                string docId = filename.substr(0, filename.size() - 5);
                docs.push_back(docId);
            }
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
    return docs;
}

} // namespace collabpad
