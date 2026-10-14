#pragma once

#include "CharId.hpp"
#include "Operation.hpp"
#include "Sequence.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

namespace collabpad {

/**
 * @brief Represents a persistent snapshot of an RGA collaborative document.
 * Contains all necessary metadata and character nodes (IDs, ordering parent IDs,
 * character values, tombstone deletion flags) and pending deletions required to
 * deterministically reconstruct a CRDT replica.
 */
struct DocumentSnapshot {
    std::string docId;
    std::string title;
    uint64_t clock{0};
    std::vector<Character> characters;
    std::vector<DeletionOp> pendingDeletions;

    DocumentSnapshot() = default;

    DocumentSnapshot(const std::string& id, const std::string& docTitle,
                     uint64_t clk, const std::vector<Character>& chars,
                     const std::vector<DeletionOp>& pendingDels = {})
        : docId(id), title(docTitle), clock(clk), characters(chars), pendingDeletions(pendingDels) {}

    std::string getText() const {
        std::string s;
        s.reserve(characters.size());
        for (const auto& ch : characters) {
            if (!ch.isDeleted) {
                s.push_back(ch.value);
            }
        }
        return s;
    }

    /**
     * @brief Serializes this snapshot into a standard JSON string.
     */
    std::string toJson() const {
        std::ostringstream ss;
        ss << "{\"docId\":\"" << escapeJson(docId) << "\","
           << "\"title\":\"" << escapeJson(title) << "\","
           << "\"clock\":" << clock << ","
           << "\"characters\":[";

        for (size_t i = 0; i < characters.size(); ++i) {
            if (i > 0) ss << ",";
            const auto& ch = characters[i];
            ss << "{\"id\":{\"siteId\":" << ch.id.siteId
               << ",\"clock\":" << ch.id.clock << "},"
               << "\"afterId\":{\"siteId\":" << ch.afterId.siteId
               << ",\"clock\":" << ch.afterId.clock << "},"
               << "\"value\":\"" << escapeChar(ch.value) << "\","
               << "\"deleted\":" << (ch.isDeleted ? "true" : "false") << "}";
        }

        ss << "],\"pendingDeletions\":[";
        for (size_t i = 0; i < pendingDeletions.size(); ++i) {
            if (i > 0) ss << ",";
            const auto& del = pendingDeletions[i];
            ss << "{\"targetId\":{\"siteId\":" << del.targetId.siteId
               << ",\"clock\":" << del.targetId.clock << "},"
               << "\"siteId\":" << del.siteId
               << ",\"clock\":" << del.clock << "}";
        }
        ss << "]}";
        return ss.str();
    }

    /**
     * @brief Parses a DocumentSnapshot from a JSON string.
     * Returns true if parsing was successful.
     */
    static bool fromJson(const std::string& json, DocumentSnapshot& out) {
        out.docId = extractString(json, "docId");
        if (out.docId.empty()) {
            return false;
        }
        out.title = extractString(json, "title");
        out.clock = extractUint64(json, "clock");
        out.characters.clear();

        // Find "characters":[ ... ]
        size_t charsPos = json.find("\"characters\"");
        if (charsPos == std::string::npos) {
            return true; // Document with 0 characters is valid
        }
        size_t openBracket = json.find('[', charsPos);
        if (openBracket == std::string::npos) {
            return false;
        }

        size_t pos = openBracket + 1;
        while (pos < json.size()) {
            size_t nextObj = json.find('{', pos);
            if (nextObj == std::string::npos) break;

            // Check if we hit closing bracket ']' before next object
            size_t closeBracket = json.find(']', pos);
            if (closeBracket != std::string::npos && closeBracket < nextObj) {
                break;
            }

            // Find end of this character object
            size_t closeObj = json.find('}', nextObj);
            if (closeObj == std::string::npos) break;

            // Character object may contain nested objects {"id":{...},"afterId":{...}}
            // So find matching brace
            int depth = 0;
            size_t objEnd = nextObj;
            for (size_t i = nextObj; i < json.size(); ++i) {
                if (json[i] == '{') depth++;
                else if (json[i] == '}') {
                    depth--;
                    if (depth == 0) {
                        objEnd = i;
                        break;
                    }
                }
            }

            std::string itemJson = json.substr(nextObj, objEnd - nextObj + 1);

            Character ch;
            ch.id.siteId = static_cast<uint32_t>(extractSubFieldUint64(itemJson, "id", "siteId"));
            ch.id.clock = extractSubFieldUint64(itemJson, "id", "clock");
            ch.afterId.siteId = static_cast<uint32_t>(extractSubFieldUint64(itemJson, "afterId", "siteId"));
            ch.afterId.clock = extractSubFieldUint64(itemJson, "afterId", "clock");
            ch.value = extractCharValue(itemJson, "value");
            ch.isDeleted = extractBool(itemJson, "deleted");

            out.characters.push_back(ch);
            pos = objEnd + 1;
        }

        // Parse "pendingDeletions":[ ... ] if present
        out.pendingDeletions.clear();
        size_t pendPos = json.find("\"pendingDeletions\"");
        if (pendPos != std::string::npos) {
            size_t pOpen = json.find('[', pendPos);
            if (pOpen != std::string::npos) {
                size_t pPos = pOpen + 1;
                while (pPos < json.size()) {
                    size_t nextObj = json.find('{', pPos);
                    if (nextObj == std::string::npos) break;

                    size_t closeBracket = json.find(']', pPos);
                    if (closeBracket != std::string::npos && closeBracket < nextObj) {
                        break;
                    }

                    int depth = 0;
                    size_t objEnd = nextObj;
                    for (size_t i = nextObj; i < json.size(); ++i) {
                        if (json[i] == '{') depth++;
                        else if (json[i] == '}') {
                            depth--;
                            if (depth == 0) {
                                objEnd = i;
                                break;
                            }
                        }
                    }

                    std::string itemJson = json.substr(nextObj, objEnd - nextObj + 1);

                    DeletionOp del;
                    if (itemJson.find("\"targetId\"") != std::string::npos) {
                        del.targetId.siteId = static_cast<uint32_t>(extractSubFieldUint64(itemJson, "targetId", "siteId"));
                        del.targetId.clock = extractSubFieldUint64(itemJson, "targetId", "clock");
                    } else {
                        del.targetId.siteId = static_cast<uint32_t>(extractUint64(itemJson, "siteId"));
                        del.targetId.clock = extractUint64(itemJson, "clock");
                    }
                    del.siteId = static_cast<uint32_t>(extractUint64(itemJson, "siteId"));
                    del.clock = extractUint64(itemJson, "clock");

                    out.pendingDeletions.push_back(del);
                    pPos = objEnd + 1;
                }
            }
        }

        return true;
    }

private:
    static std::string escapeJson(const std::string& s) {
        std::string res;
        res.reserve(s.size() + 8);
        for (char c : s) {
            if (c == '"') res += "\\\"";
            else if (c == '\\') res += "\\\\";
            else if (c == '\n') res += "\\n";
            else if (c == '\r') res += "\\r";
            else if (c == '\t') res += "\\t";
            else res.push_back(c);
        }
        return res;
    }

    static std::string escapeChar(char c) {
        if (c == '"') return "\\\"";
        if (c == '\\') return "\\\\";
        if (c == '\n') return "\\n";
        if (c == '\r') return "\\r";
        if (c == '\t') return "\\t";
        return std::string(1, c);
    }

    static std::string extractString(const std::string& json, const std::string& key) {
        std::string needle = "\"" + key + "\"";
        size_t kpos = json.find(needle);
        if (kpos == std::string::npos) return "";

        size_t colon = json.find(':', kpos + needle.size());
        if (colon == std::string::npos) return "";

        size_t vstart = colon + 1;
        while (vstart < json.size() && (json[vstart] == ' ' || json[vstart] == '\t'
               || json[vstart] == '\r' || json[vstart] == '\n')) {
            ++vstart;
        }
        if (vstart >= json.size() || json[vstart] != '"') return "";
        ++vstart;

        std::string val;
        for (size_t i = vstart; i < json.size(); ++i) {
            if (json[i] == '\\' && i + 1 < json.size()) {
                char next = json[i + 1];
                if (next == 'n') val.push_back('\n');
                else if (next == 'r') val.push_back('\r');
                else if (next == 't') val.push_back('\t');
                else val.push_back(next);
                ++i;
            } else if (json[i] == '"') {
                break;
            } else {
                val.push_back(json[i]);
            }
        }
        return val;
    }

    static uint64_t extractUint64(const std::string& json, const std::string& key) {
        std::string needle = "\"" + key + "\"";
        size_t kpos = json.find(needle);
        if (kpos == std::string::npos) return 0;

        size_t colon = json.find(':', kpos + needle.size());
        if (colon == std::string::npos) return 0;

        size_t vstart = colon + 1;
        while (vstart < json.size() && (json[vstart] == ' ' || json[vstart] == '\t')) {
            ++vstart;
        }

        uint64_t res = 0;
        while (vstart < json.size() && json[vstart] >= '0' && json[vstart] <= '9') {
            res = res * 10 + (json[vstart] - '0');
            ++vstart;
        }
        return res;
    }

    static uint64_t extractSubFieldUint64(const std::string& json,
                                          const std::string& objKey,
                                          const std::string& subKey) {
        std::string needle = "\"" + objKey + "\"";
        size_t kpos = json.find(needle);
        if (kpos == std::string::npos) return 0;

        size_t openBrace = json.find('{', kpos + needle.size());
        if (openBrace == std::string::npos) return 0;
        size_t closeBrace = json.find('}', openBrace);
        if (closeBrace == std::string::npos) return 0;

        std::string subObj = json.substr(openBrace, closeBrace - openBrace + 1);
        return extractUint64(subObj, subKey);
    }

    static char extractCharValue(const std::string& json, const std::string& key) {
        std::string needle = "\"" + key + "\"";
        size_t kpos = json.find(needle);
        if (kpos == std::string::npos) return '\0';

        size_t colon = json.find(':', kpos + needle.size());
        if (colon == std::string::npos) return '\0';

        size_t vstart = colon + 1;
        while (vstart < json.size() && (json[vstart] == ' ' || json[vstart] == '\t')) {
            ++vstart;
        }
        if (vstart >= json.size() || json[vstart] != '"') return '\0';
        ++vstart;

        if (vstart < json.size()) {
            if (json[vstart] == '\\' && vstart + 1 < json.size()) {
                char next = json[vstart + 1];
                if (next == 'n') return '\n';
                if (next == 'r') return '\r';
                if (next == 't') return '\t';
                return next;
            }
            return json[vstart];
        }
        return '\0';
    }

    static bool extractBool(const std::string& json, const std::string& key) {
        std::string needle = "\"" + key + "\"";
        size_t kpos = json.find(needle);
        if (kpos == std::string::npos) return false;

        size_t colon = json.find(':', kpos + needle.size());
        if (colon == std::string::npos) return false;

        size_t vstart = colon + 1;
        while (vstart < json.size() && (json[vstart] == ' ' || json[vstart] == '\t')) {
            ++vstart;
        }
        if (vstart + 4 <= json.size() && json.substr(vstart, 4) == "true") {
            return true;
        }
        return false;
    }
};

} // namespace collabpad
