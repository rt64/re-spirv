//
// re-spirv
//

#include "re-spirv.h"

#include <filesystem>
#include <fstream>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "./re-spirv-cli <spirv-input-file> <spirv-output-file>");
        return 1;
    }
    
    const char *inputPath = argv[1];
    const char *outputPath = argv[2];
    std::ifstream inputStream(std::filesystem::u8path(inputPath), std::ios::binary);
    if (!inputStream.is_open()) {
        fprintf(stderr, "Failed to open %s.\n", inputPath);
        return 1;
    }

    std::vector<char> fileData;
    inputStream.seekg(0, std::ios::end);
    size_t fileSize = inputStream.tellg();
    inputStream.seekg(0, std::ios::beg);
    fileData.resize(fileSize);
    inputStream.read(fileData.data(), fileSize);
    if (inputStream.bad()) {
        fprintf(stderr, "Failed to read %s.\n", inputPath);
        return 1;
    }

    auto beginParsingTime = std::chrono::high_resolution_clock::now();
    respv::Optimizer optimizer;
    if (!optimizer.parse(fileData.data(), fileData.size())) {
        fprintf(stderr, "Failed to parse SPIR-V data from %s.\n", inputPath);
        return 1;
    }

    auto beginRunTime = std::chrono::high_resolution_clock::now();
    std::vector<uint8_t> optimizedData;
    std::vector<respv::SpecConstant> specConstants = optimizer.getSpecConstants();

    ///
    specConstants[0].values[0] = 0x10;
    specConstants[1].values[0] = 0x20;
    specConstants[2].values[0] = 0x40;
    specConstants[3].values[0] = 0x80;
    specConstants[4].values[0] = 0x100;
    ///

    if (!optimizer.run(specConstants.data(), specConstants.size(), optimizedData)) {
        fprintf(stderr, "Failed to optimize SPIR-V data from %s.\n", inputPath);
        return 1;
    }

    auto finishTime = std::chrono::high_resolution_clock::now();
    double parsingTime = std::chrono::duration_cast<std::chrono::microseconds>(beginRunTime - beginParsingTime).count() / 1000.0f;
    double optimizationTime = std::chrono::duration_cast<std::chrono::microseconds>(finishTime - beginRunTime).count() / 1000.0f;
    fprintf(stdout, "Parsing time: %f ms\n", parsingTime);
    fprintf(stdout, "Optimization time: %f ms\n", optimizationTime);

    std::ofstream outputStream(std::filesystem::u8path(outputPath), std::ios::binary);
    if (!outputStream.is_open()) {
        fprintf(stderr, "Failed to open %s for writing.\n", outputPath);
        return 1;
    }

    outputStream.write(reinterpret_cast<const char *>(optimizedData.data()), optimizedData.size());
    if (outputStream.bad()) {
        outputStream.close();
        std::filesystem::remove(std::filesystem::u8path(outputPath));
        fprintf(stderr, "Failed to write to %s.\n", outputPath);
        return 1;
    }

    outputStream.close();
    fprintf(stdout, "Saved result to %s.\n", outputPath);

    return 0;
}