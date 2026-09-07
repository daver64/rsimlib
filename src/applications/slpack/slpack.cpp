#include <zlib.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct ZipEntry {
    std::string name;
    std::vector<std::uint8_t> data;
    std::vector<std::uint8_t> compressed;
    std::uint32_t crc = 0;
    std::uint32_t offset = 0;
    std::uint16_t method = 0;
};

struct PackStats {
    std::size_t files = 0;
    std::size_t directories = 0;
};

void append16(std::vector<std::uint8_t>& output, std::uint16_t value) {
    output.push_back(static_cast<std::uint8_t>(value));
    output.push_back(static_cast<std::uint8_t>(value >> 8));
}

void append32(std::vector<std::uint8_t>& output, std::uint32_t value) {
    append16(output, static_cast<std::uint16_t>(value));
    append16(output, static_cast<std::uint16_t>(value >> 16));
}

bool read_file(const std::filesystem::path& path, std::vector<std::uint8_t>& data) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return false;
    const auto size = file.tellg();
    if (size < 0) return false;
    data.resize(static_cast<std::size_t>(size));
    file.seekg(0);
    return data.empty() || static_cast<bool>(file.read(reinterpret_cast<char*>(data.data()), size));
}

bool deflate_data(const std::vector<std::uint8_t>& input, std::vector<std::uint8_t>& output) {
    z_stream stream{};
    if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) return false;
    output.resize(compressBound(static_cast<uLong>(input.size())));
    stream.next_in = const_cast<Bytef*>(input.data());
    stream.avail_in = static_cast<uInt>(input.size());
    stream.next_out = output.data();
    stream.avail_out = static_cast<uInt>(output.size());
    const int result = deflate(&stream, Z_FINISH);
    if (result != Z_STREAM_END) {
        deflateEnd(&stream);
        return false;
    }
    output.resize(stream.total_out);
    deflateEnd(&stream);
    return true;
}

std::string archive_name(const std::filesystem::path& path) {
    return path.generic_string();
}

bool add_path(const std::filesystem::path& input, const std::filesystem::path& base, std::vector<ZipEntry>& entries, PackStats& stats) {
    std::error_code error;
    if (std::filesystem::is_directory(input, error)) {
        const std::string directory = archive_name(std::filesystem::relative(input, base, error)) + "/";
        if (error) return false;
        if (directory != "./") {
            entries.push_back({directory});
            ++stats.directories;
        }
        for (const auto& child : std::filesystem::directory_iterator(input, error)) {
            if (error || !add_path(child.path(), base, entries, stats)) return false;
        }
        return true;
    }
    if (!std::filesystem::is_regular_file(input, error) || error) return false;
    ZipEntry entry;
    entry.name = archive_name(std::filesystem::relative(input, base, error));
    if (error || !read_file(input, entry.data)) return false;
    std::cout << "file: " << entry.name << '\n';
    ++stats.files;
    entry.crc = crc32(0, entry.data.data(), static_cast<uInt>(entry.data.size()));
    if (deflate_data(entry.data, entry.compressed) && entry.compressed.size() < entry.data.size()) entry.method = 8;
    else entry.compressed = entry.data;
    entries.push_back(std::move(entry));
    return true;
}

bool write_archive(const std::filesystem::path& outputPath, std::vector<ZipEntry>& entries) {
    std::vector<std::uint8_t> archive;
    for (auto& entry : entries) {
        if (entry.name.size() > UINT16_MAX || entry.data.size() > UINT32_MAX || entry.compressed.size() > UINT32_MAX || archive.size() > UINT32_MAX) return false;
        entry.offset = static_cast<std::uint32_t>(archive.size());
        append32(archive, 0x04034b50);
        append16(archive, 20);
        append16(archive, 0);
        append16(archive, entry.method);
        append16(archive, 0);
        append16(archive, 0);
        append32(archive, entry.crc);
        append32(archive, static_cast<std::uint32_t>(entry.compressed.size()));
        append32(archive, static_cast<std::uint32_t>(entry.data.size()));
        append16(archive, static_cast<std::uint16_t>(entry.name.size()));
        append16(archive, 0);
        archive.insert(archive.end(), entry.name.begin(), entry.name.end());
        archive.insert(archive.end(), entry.compressed.begin(), entry.compressed.end());
    }
    const std::uint32_t directoryOffset = static_cast<std::uint32_t>(archive.size());
    for (const auto& entry : entries) {
        append32(archive, 0x02014b50);
        append16(archive, 20);
        append16(archive, 20);
        append16(archive, 0);
        append16(archive, entry.method);
        append16(archive, 0);
        append16(archive, 0);
        append32(archive, entry.crc);
        append32(archive, static_cast<std::uint32_t>(entry.compressed.size()));
        append32(archive, static_cast<std::uint32_t>(entry.data.size()));
        append16(archive, static_cast<std::uint16_t>(entry.name.size()));
        append16(archive, 0);
        append16(archive, 0);
        append16(archive, 0);
        append16(archive, 0);
        append32(archive, entry.name.back() == '/' ? 0x10 : 0);
        append32(archive, entry.offset);
        archive.insert(archive.end(), entry.name.begin(), entry.name.end());
    }
    const std::uint32_t directorySize = static_cast<std::uint32_t>(archive.size()) - directoryOffset;
    append32(archive, 0x06054b50);
    append16(archive, 0);
    append16(archive, 0);
    append16(archive, static_cast<std::uint16_t>(entries.size()));
    append16(archive, static_cast<std::uint16_t>(entries.size()));
    append32(archive, directorySize);
    append32(archive, directoryOffset);
    append16(archive, 0);

    std::ofstream file(outputPath, std::ios::binary);
    return file && static_cast<bool>(file.write(reinterpret_cast<const char*>(archive.data()), archive.size()));
}

} // namespace

/** Pack files and folders into a standard ZIP datafile. */
int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "usage: slpack output.zip input...\n";
        return 2;
    }
    const std::filesystem::path outputPath = argv[1];
    std::vector<ZipEntry> entries;
    PackStats stats;
    for (int index = 2; index < argc; ++index) {
        const std::filesystem::path input = std::filesystem::absolute(argv[index]);
        if (!std::filesystem::exists(input)) {
            std::cerr << "input does not exist: " << input << '\n';
            return 1;
        }
        const std::filesystem::path base = input.parent_path();
        if (!add_path(input, base, entries, stats)) {
            std::cerr << "unable to read input: " << input << '\n';
            return 1;
        }
    }
    if (!write_archive(outputPath, entries)) {
        std::cerr << "unable to write archive: " << outputPath << '\n';
        return 1;
    }
    std::cout << "packed " << stats.files << " files and " << stats.directories
              << " directories (" << entries.size() << " entries) into " << outputPath << '\n';
    return 0;
}
