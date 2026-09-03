#include "error.h"
#include "resource.h"

#include <cassert>
#include <cstdint>
#include <string>

int main(int argc, char* argv[]) {
    assert(argc == 2);

    simlib::Archive archive;
    assert(archive.open(argv[1]));
    assert(archive.contains("stored.txt"));
    assert(archive.contains("deflated.txt"));
    assert(!archive.contains("missing.txt"));
    assert(archive.entries().size() == 2);

    const auto stored = archive.read("stored.txt");
    const auto deflated = archive.read("deflated.txt");
    assert(std::string(stored.begin(), stored.end()) == "stored entry\n");
    assert(std::string(deflated.begin(), deflated.end()) == "deflated entry with enough repeated text to exercise compression\n");

    assert(archive.read("missing.txt").empty());
    assert(!simlib::last_error().empty());
    archive.close();
    assert(!archive.contains("stored.txt"));
    return 0;
}
