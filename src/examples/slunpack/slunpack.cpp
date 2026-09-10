#include "resource.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

/** Unpack a ZIP datafile created by slpack. */
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "usage: slunpack archive.zip [output_dir]\n";
        return 2;
    }

    const std::filesystem::path archivePath = argv[1];
    const std::filesystem::path outputDir = argc > 2 ? argv[2] : std::filesystem::path(".");

    sl::Archive archive;
    if (!archive.open(archivePath.string())) {
        std::cerr << "unable to open archive: " << archivePath << '\n';
        return 1;
    }

    std::size_t files = 0;
    std::size_t directories = 0;
    for (const std::string& name : archive.entries()) {
        const std::filesystem::path destination = outputDir / name;
        if (!name.empty() && name.back() == '/') {
            std::error_code error;
            std::filesystem::create_directories(destination, error);
            ++directories;
            continue;
        }

        std::error_code error;
        std::filesystem::create_directories(destination.parent_path(), error);

        const std::vector<std::uint8_t> data = archive.read(name);
        std::ofstream file(destination, std::ios::binary);
        if (!file || !file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()))) {
            std::cerr << "unable to write file: " << destination << '\n';
            return 1;
        }
        std::cout << "file: " << name << '\n';
        ++files;
    }

    std::cout << "unpacked " << files << " files and " << directories
              << " directories from " << archivePath << " into " << outputDir << '\n';
    return 0;
}
