#pragma once
// Host stand-in for the ESP32 NVS Preferences library: one in-memory store
// shared by every instance, so save/load round trips can be tested.
#include <algorithm>
#include <map>
#include <string>
struct Preferences {
    static std::map<std::string, std::map<std::string, float>> &store() {
        static std::map<std::string, std::map<std::string, float>> data;
        return data;
    }
    std::string space;
    bool open = false;
    bool begin(const char *name, bool readOnly = false) {
        if (readOnly && !store().count(name)) {
            return false;
        }
        space = name;
        store()[space];
        open = true;
        return true;
    }
    void end() { open = false; }
    float getFloat(const char *key, float fallback) {
        auto &s = store()[space];
        auto it = s.find(key);
        return it == s.end() ? fallback : it->second;
    }
    size_t putFloat(const char *key, float value) { store()[space][key] = value; return 4; }
    bool clear() { store()[space].clear(); strings()[space].clear(); return true; }
    static std::map<std::string, std::map<std::string, std::string>> &strings() {
        static std::map<std::string, std::map<std::string, std::string>> data;
        return data;
    }
    size_t putString(const char *key, const char *value) { strings()[space][key] = value; return 1; }
    size_t getString(const char *key, char *out, size_t maxLength) {
        auto &s = strings()[space];
        auto it = s.find(key);
        if (it == s.end() || maxLength == 0) return 0;
        const size_t n = std::min(maxLength - 1, it->second.size());
        it->second.copy(out, n);
        out[n] = '\0';
        return n + 1;
    }
};
