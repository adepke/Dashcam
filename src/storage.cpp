#include "storage.h"

#include <cstring>
#include <time.h>
#include <set>
#include <filesystem>
#include <iostream>

std::string getDateTime() {
    time_t now = time(0);
    auto t = *localtime(&now);
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d_%H:%M:%S", &t);

    return buffer;
}

FILE* getStorage(FILE* oldFile, size_t& space) {
    constexpr size_t bufferSpace = 512ULL * 1024ULL * 1024ULL;  // 512 MB
    constexpr size_t maxFileSize = 512ULL * 1024ULL * 1024ULL;  // 512 MB

    if (oldFile) {
        fclose(oldFile);
    }

    // Determine max storage space.
    auto freeSpace = std::filesystem::space(storageLocation).available - bufferSpace;

    while (freeSpace < maxFileSize) {
        // Delete the oldest recording, by file name.
        std::set<std::filesystem::path> entries;
        for (const auto& entry : std::filesystem::directory_iterator{ storageLocation }) {
            entries.insert(entry.path());
        }

        if (entries.size() == 0) {
            std::cerr << "Could not find any files to remove from the storage location '" << storageLocation << "'. Not enough "
                << "space to accommodate a full video. Free space: " << freeSpace << ", requested: "
                << maxFileSize << "\n";
            return nullptr;
        }

        auto target = *entries.begin();
        if (!std::filesystem::remove(target)) {
            std::cerr << "Failed to remove file: '" << target << "'\n";
            return nullptr;
        }

        freeSpace = std::filesystem::space(storageLocation).available - bufferSpace;
    }

    auto fileName = storageLocation + getDateTime() + ".h264";

    FILE* outFile = fopen(fileName.c_str(), "w+");
    if (!outFile) {
        std::cerr << "Failed to create output video.\n";
        return nullptr;
    }

    std::cout << "Created new file: '" << fileName.c_str() << "', max size of " << maxFileSize << "\n";

    space = maxFileSize;

    return outFile;
}
