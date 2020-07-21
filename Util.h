#pragma once
#include <vector>
#include <string>
#include <fstream>

#include "Types.h"

inline std::vector<char> read_file(std::string const & filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        printf("Unable to open file '%s'!", filename.c_str());
        abort();
    }

    u64 file_size = file.tellg();
    std::vector<char> buffer(file_size);

    file.seekg(0);
    file.read(buffer.data(), file_size);
    file.close();

    return buffer;
}
