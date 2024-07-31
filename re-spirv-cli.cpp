//
// re-spirv
//

#include "re-spirv.h"

#include <filesystem>
#include <fstream>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "./re-spirv-cli <spirv-file>");
        return 1;
    }
    
    std::ifstream fileStream(std::filesystem::u8path(argv[0]));
    if (!fileStream.is_open()) {
        fprintf(stderr, "Failed to open %s.\n", argv[0]);
        return 1;
    }

    std::vector<char> fileData;
    fileStream.seekg(0, std::ios::end);
    size_t fileSize = fileStream.tellg();
    fileStream.seekg(0, std::ios::beg);
    fileData.resize(fileSize);
    fileStream.read(fileData.data(), fileSize);
    if (fileStream.bad()) {
        fprintf(stderr, "Failed to read %s.\n", argv[0]);
        return 1;
    }

    respv::Optimizer optimizer;
    if (!optimizer.parse(fileData.data(), fileData.size())) {
        fprintf(stderr, "Failed to parse SPIR-V data from %s.\n", argv[0]);
        return 1;
    }

    std::vector<uint8_t> optimizedData;
    std::vector<respv::SpecConstant> specConstants = optimizer.getDefaultSpecConstants();
    if (!optimizer.run(specConstants.data(), specConstants.size(), optimizedData)) {
        fprintf(stderr, "Failed to optimize SPIR-V data from %s.\n", argv[0]);
        return 1;
    }



    return 0;
}