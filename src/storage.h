#pragma once

#include <stdio.h>

constexpr const char* storageLocation = "./data/";

FILE* getStorage(FILE* oldFile, size_t& space);
