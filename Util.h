#pragma once
#include <vector>
#include <string>
#include <fstream>

#include "Types.h"

#define FORCEINLINE __forceinline

#define PI          3.14159265359f
#define ONE_OVER_PI 0.31830988618f

#define TWO_PI          6.28318530718f
#define ONE_OVER_TWO_PI 0.15915494309f

#define DEG_TO_RAD(angle) ((angle) * PI / 180.0f)
#define RAD_TO_DEG(angle) ((angle) / PI * 180.0f)

namespace Util {
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

	template<typename T, int N>
	constexpr int array_element_count(const T (& array)[N]) {
		return N;
	}
}