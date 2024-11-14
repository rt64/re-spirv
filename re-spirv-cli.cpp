//
// re-spirv
//
// Copyright (c) 2024 renderbag and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE file for details.
//

#include "re-spirv.h"

#include <set>
#include <filesystem>
#include <fstream>

#if defined(BATCH_FOLDER)

int main(int argc, char *argv[]) {
    std::filesystem::path folder = BATCH_FOLDER;
    std::vector<char> fileData;
    std::vector<respv::SpecConstant> specConstants;
    std::set<uint32_t> specIds;
    for (std::filesystem::directory_entry entry : std::filesystem::directory_iterator(folder)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const std::string SpirvExtension = ".spirv";
        if (entry.path().extension() != SpirvExtension) {
            continue;
        }

        std::string spirvPathU8 = entry.path().u8string();
        std::ifstream spirvStream(entry.path(), std::ios::binary);
        spirvStream.seekg(0, std::ios::end);
        size_t fileSize = spirvStream.tellg();
        spirvStream.seekg(0, std::ios::beg);
        fileData.resize(fileSize);
        spirvStream.read(fileData.data(), fileSize);
        if (spirvStream.bad()) {
            fprintf(stderr, "Failed to read %s.\n", spirvPathU8.c_str());
            return 1;
        }

        spirvStream.close();

        respv::Shader shader;
        if (!shader.parse(fileData.data(), fileData.size())) {
            fprintf(stderr, "Failed to parse SPIR-V data from %s.\n", spirvPathU8.c_str());
            return 1;
        }

        std::filesystem::path specPath = std::filesystem::u8path(spirvPathU8.substr(0, spirvPathU8.size() - SpirvExtension.size()) + ".spec");
        std::string specPathU8 = specPath.u8string();
        std::ifstream specStream(specPath, std::ios::binary);
        if (specStream.bad()) {
            fprintf(stderr, "Failed to read %s.\n", specPathU8.c_str());
            return 1;
        }

        specConstants.clear();
        specIds.clear();
        while (!specStream.eof()) {
            uint32_t constantId = 0;
            uint32_t constantValue = 0;
            specStream.read((char *)(&constantId), sizeof(uint32_t));
            specStream.read((char *)(&constantValue), sizeof(uint32_t));
            if (specStream.eof()) {
                break;
            }
            else if (specStream.bad()) {
                fprintf(stderr, "Failed to read %s.\n", specPathU8.c_str());
                return 1;
            }
            else if (specIds.find(constantId) != specIds.end()) {
                fprintf(stderr, "Found duplicate constant %u in %s.\n", constantId, specPathU8.c_str());
                return 1;
            }
            else {
                specIds.insert(constantId);
                specConstants.emplace_back(constantId, std::vector{ constantValue });
            }
        }

        specStream.close();

        std::vector<uint8_t> optimizedData;
        if (!respv::Optimizer::run(shader, specConstants.data(), specConstants.size(), optimizedData)) {
            fprintf(stderr, "Failed to optimize SPIR-V data from %s.\n", spirvPathU8.c_str());
            return 1;
        }

        std::filesystem::path optPath = std::filesystem::u8path(spirvPathU8 + ".opt");
        std::string optPathU8 = optPath.u8string();
        std::ofstream outputStream(optPath, std::ios::binary);
        if (!outputStream.is_open()) {
            fprintf(stderr, "Failed to open %s for writing.\n", optPathU8.c_str());
            return 1;
        }

        outputStream.write(reinterpret_cast<const char *>(optimizedData.data()), optimizedData.size());
        if (outputStream.bad()) {
            outputStream.close();
            std::filesystem::remove(optPath);
            fprintf(stderr, "Failed to write to %s.\n", optPathU8.c_str());
            return 1;
        }

        outputStream.close();
        fprintf(stdout, "Saved result to %s.\n", optPathU8.c_str());

        std::string systemCommand = "spirv-val " + optPathU8;
        int resultCode = std::system(systemCommand.c_str());
        if (resultCode != 0) {
            fprintf(stderr, "Failed to validate %s. Result code %d.\n", optPathU8.c_str(), resultCode);
            return 1;
        }
    }

    return 0;
}

#else

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
    respv::Shader shader;
    if (!shader.parse(fileData.data(), fileData.size())) {
        fprintf(stderr, "Failed to parse SPIR-V data from %s.\n", inputPath);
        return 1;
    }
    
    auto endParsingTime = std::chrono::high_resolution_clock::now();
    std::vector<uint8_t> optimizedData;
    std::vector<respv::SpecConstant> specConstants = {
        respv::SpecConstant(0, { 3356565624U }),
        respv::SpecConstant(1, { 1584128U }),
        respv::SpecConstant(2, { 4229999620U }),
        respv::SpecConstant(3, { 4279211007U }),
        respv::SpecConstant(4, { 747626510U }),
    };

    auto beginRunTime = std::chrono::high_resolution_clock::now();
    if (!respv::Optimizer::run(shader, specConstants.data(), specConstants.size(), optimizedData)) {
        fprintf(stderr, "Failed to optimize SPIR-V data from %s.\n", inputPath);
        return 1;
    }

    auto endRunTime = std::chrono::high_resolution_clock::now();
    double parsingTime = std::chrono::duration_cast<std::chrono::microseconds>(endParsingTime - beginParsingTime).count() / 1000.0f;
    double optimizationTime = std::chrono::duration_cast<std::chrono::microseconds>(endRunTime - beginRunTime).count() / 1000.0f;
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

#endif