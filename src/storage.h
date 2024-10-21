#pragma once

#include <stdio.h>

constexpr const char* storageLocation = "./data/";

struct Storage {
    size_t space = 0;
    FILE* file = nullptr;
};

Storage getStorage(Storage& oldStorage);
