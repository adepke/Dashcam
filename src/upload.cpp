#include "upload.h"
#include "storage.h"

#include <iostream>
#include <filesystem>
#include <set>
#include <cstdio>

int uploadMedia() {
    // Find all clips and upload them to the remote storage.
    // TODO: Pipeline conversion and uploading to greatly speed this up.
    int failures = 0;

    for (const auto& entry : std::filesystem::directory_iterator{ storageLocation }) {
        std::cout << "Converting " << entry << " to MP4...\n";
        ++failures;

        const auto convertCommand = "./python/convert.py --file " + entry.path().string() + " --dest " + storageLocation;
        auto returnCode = system(convertCommand.c_str());

        std::array<char, 128> convertBuffer;
        std::string convertStdout;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(convertCommand.c_str(), "r"), pclose);
        if (!pipe) {
            std::cerr << "Failed to launch conversion subprocess!\n";
            continue;
        }

        while (fgets(convertBuffer.data(), convertBuffer.size(), pipe.get()) != nullptr) {
            convertStdout += convertBuffer.data();
        }

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

        auto convertedPath = std::filesystem::path(convertStdout);
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
