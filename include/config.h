#pragma once

#include <string>

#include "json.hpp"

namespace simlib {

/** A JSON-backed application configuration document. */
class Config {
public:
    /** Load a JSON configuration document from disk. */
    bool load(const std::string& path);
    /** Save the configuration document to disk. */
    bool save(const std::string& path, int indentation = 4) const;
    /** Return whether a dotted key path exists. */
    bool contains(const std::string& key) const;
    /** Return a typed value, or the supplied default when absent or invalid. */
    template <typename Type>
    Type get(const std::string& key, const Type& fallback) const {
        try {
            const nlohmann::json* value = &document_;
            std::size_t start = 0;
            while (start < key.size()) {
                const std::size_t separator = key.find('.', start);
                const std::string part = key.substr(start, separator - start);
                if (!value->is_object() || !value->contains(part)) return fallback;
                value = &(*value)[part];
                if (separator == std::string::npos) break;
                start = separator + 1;
            }
            return value->get<Type>();
        } catch (...) {
            return fallback;
        }
    }
    /** Set a value at a dotted key path, creating objects as necessary. */
    template <typename Type>
    void set(const std::string& key, const Type& value) {
        nlohmann::json* target = &document_;
        std::size_t start = 0;
        for (;;) {
            const std::size_t separator = key.find('.', start);
            const std::string part = key.substr(start, separator - start);
            if (separator == std::string::npos) {
                (*target)[part] = value;
                return;
            }
            target = &(*target)[part];
            start = separator + 1;
        }
    }
    /** Return the complete JSON document. */
    const nlohmann::json& document() const;

private:
    nlohmann::json document_ = nlohmann::json::object();
};

} // namespace simlib
