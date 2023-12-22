#include "upload.h"
#include "storage.h"

#include <iostream>
#include <filesystem>
#include <set>

int uploadMedia() {
    // Find all clips and upload them to the remote storage.
    // TODO: Pipeline conversion and uploading to greatly speed this up.
    int failures = 0;

    for (const auto& entry : std::filesystem::directory_iterator{ storageLocation }) {
        std::cout << "Converting " << entry << " to MP4...\n";
        ++failures;

        const auto convertCommand = "./python/convert.py --file " + entry.path().string();
        auto returnCode = system(convertCommand.c_str());

        if (returnCode != 0) {
            std::cerr << "Failed to convert! Code: " << returnCode << "\n";
            continue;
        }

        // Delete the source clip.
        if (!std::filesystem::remove(entry.path())) {
            std::cerr << "Failed to delete source file!\n";
            continue;
        }

        std::cout << "Uploading...\n";

        auto convertedPath = entry.path();
        convertedPath.replace_extension(".mp4");
        const auto uploadCommand = "./python/upload.py --file " + convertedPath.string();
        returnCode = system(uploadCommand.c_str());

        if (returnCode != 0) {
            std::cerr << "Failed to upload! Code: " << returnCode << "\n";
            continue;
        }

        // Delete the converted clip.
        if (!std::filesystem::remove(convertedPath)) {
            std::cerr << "Failed to delete converted file!\n";
            continue;
        }

        --failures;
        std::cout << "Success!\n";
    }

    return failures;
}
