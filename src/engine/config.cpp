#include "config.h"

#include "error.h"

#include <fstream>

namespace simlib {

bool Config::load(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        simlib::detail::set_error("Unable to open configuration: " + path);
        return false;
    }
    try {
        file >> document_;
        if (!document_.is_object()) {
            simlib::detail::set_error("Configuration root must be a JSON object");
            return false;
        }
        return true;
    } catch (const nlohmann::json::exception& exception) {
        simlib::detail::set_error("Invalid JSON configuration: " + std::string(exception.what()));
        return false;
    }
}

bool Config::save(const std::string& path, int indentation) const {
    std::ofstream file(path);
    if (!file) {
        simlib::detail::set_error("Unable to write configuration: " + path);
        return false;
    }
    file << document_.dump(indentation) << '\n';
    return static_cast<bool>(file);
}

bool Config::contains(const std::string& key) const {
    try {
        const nlohmann::json* value = &document_;
        std::size_t start = 0;
        while (start < key.size()) {
            const std::size_t separator = key.find('.', start);
            const std::string part = key.substr(start, separator - start);
            if (!value->is_object() || !value->contains(part)) return false;
            value = &(*value)[part];
            if (separator == std::string::npos) break;
            start = separator + 1;
        }
        return true;
    } catch (...) {
        return false;
    }
}

const nlohmann::json& Config::document() const {
    return document_;
}

} // namespace simlib
