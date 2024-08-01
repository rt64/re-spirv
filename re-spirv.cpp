//
// re-spirv
//

#include "re-spirv.h"

#include <cassert>

#define SPV_ENABLE_UTILITY_CODE

#include "spirv/unified1/spirv.h"

// TODO: Replace stderr handling with a consumer printer class.

namespace respv {
    // Optimizer

    Optimizer::Optimizer() {
        // Empty.
    }

    Optimizer::Optimizer(const void *data, size_t size) {
        parse(data, size);
    }

    bool Optimizer::parse(const void *data, size_t size) {
        assert(data != nullptr);
        assert((size % sizeof(uint32_t) == 0) && "Size of data must be aligned to the word size.");

        spirvWords = reinterpret_cast<const uint32_t *>(data);
        spirvWordCount = size / sizeof(uint32_t);
        specConstants.clear();
        specConstantsTargetIds.clear();
        decoratorIndices.clear();
        specIdToConstantIndex.clear();

        const uint32_t startingWordIndex = 5;
        if (spirvWordCount < startingWordIndex) {
            fprintf(stderr, "Not enough words in SPIR-V.\n");
            return false;
        }

        if (spirvWords[0] != SpvMagicNumber) {
            fprintf(stderr, "Invalid SPIR-V Magic Number on header.\n");
            return false;
        }

        if (spirvWords[1] > SpvVersion) {
            fprintf(stderr, "SPIR-V Version is too new for the library. Max version for the library is 0x%X.\n", SpvVersion);
            return false;
        }

        const uint32_t idBound = spirvWords[3];
        resultIndices.resize(idBound, UINT32_MAX);
        resultIndices.shrink_to_fit();

        // Parse all instructions.
        SpvOp opCode = SpvOpNop;
        uint16_t wordCount = 0;
        uint32_t wordIndex = startingWordIndex;
        uint32_t resultId = 0;
        bool hasResult = false;
        bool hasType = false;
        while (wordIndex < spirvWordCount) {
            opCode = SpvOp(spirvWords[wordIndex] & 0xFFFFU);
            wordCount = (spirvWords[wordIndex] >> 16U) & 0xFFFFU;

            SpvHasResultAndType(opCode, &hasResult, &hasType);

            if (hasResult) {
                resultId = spirvWords[wordIndex + (hasType ? 2 : 1)];
                if (resultId >= idBound) {
                    fprintf(stderr, "SPIR-V Parsing error. Invalid Result ID: %u.\n", resultId);
                    return false;
                }

                assert(resultIndices[resultId] == UINT32_MAX && "Two instructions can't write to the same result.");
                resultIndices[resultId] = wordIndex;
            }

            switch (opCode) {
            case SpvOpDecorate:
                decoratorIndices.emplace_back(wordIndex);
                break;
            default:
                // Ignore the rest.
                break;
            }

            if (wordCount == 0) {
                fprintf(stderr, "Unknown SPIR-V Parsing error.\n");
                return false;
            }

            wordIndex += wordCount;
        }

        // Parse all decorations once all instructions have been parsed.
        std::vector<uint32_t> specValues;
        for (uint32_t wordIndex : decoratorIndices) {
            uint32_t decoration = spirvWords[wordIndex + 2];
            switch (decoration) {
            case SpvDecorationSpecId: {
                uint32_t targetId = spirvWords[wordIndex + 1];
                uint32_t specWordIndex = resultIndices[targetId];
                if (specWordIndex == UINT32_MAX) {
                    fprintf(stderr, "SPIR-V Parsing error. SpvDecorationSpecId targets %u which hasn't been defined before it.\n", targetId);
                    return false;
                }

                SpvOp specOpCode = SpvOp(spirvWords[specWordIndex] & 0xFFFFU);
                uint32_t specOpWordCount = (spirvWords[specWordIndex] >> 16U) & 0xFFFFU;
                switch (specOpCode) {
                case SpvOpSpecConstantTrue:
                    specValues.resize(1);
                    specValues[0] = 1;
                    break;
                case SpvOpSpecConstantFalse:
                    specValues.resize(1);
                    specValues[0] = 0;
                    break;
                case SpvOpSpecConstant:
                    specValues.resize(specOpWordCount - 3);
                    memcpy(specValues.data(), &spirvWords[specWordIndex + 3], sizeof(uint32_t) * specValues.size());
                    break;
                case SpvOpSpecConstantComposite:
                    fprintf(stderr, "SPIR-V Parsing error. SpvOpSpecConstantComposite is not supported yet.\n");
                    return false;
                case SpvOpSpecConstantOp:
                    fprintf(stderr, "SPIR-V Parsing error. SpvOpSpecConstantOp is not supported yet.\n");
                    return false;
                default:
                    fprintf(stderr, "SPIR-V Parsing error. SpvDecorationSpecId targets opCode %u which is not valid.\n", specOpCode);
                    return false;
                }

                uint32_t constantId = spirvWords[wordIndex + 3];
                specConstants.emplace_back(constantId, specValues);
                specConstantsTargetIds.emplace_back(targetId);
                specIdToConstantIndex.resize(std::max(specIdToConstantIndex.size(), size_t(constantId + 1)), UINT32_MAX);
                specIdToConstantIndex[constantId] = uint32_t(specConstants.size() - 1);
                break;
            }
            default:
                break;
            }
        }
        
        // Indicate the data has been parsed and filled in correctly.
        filledData = true;

        return true;
    }

    bool Optimizer::empty() const {
        return !filledData;
    }

    const std::vector<SpecConstant> &Optimizer::getSpecConstants() const {
        return specConstants;
    }

    bool Optimizer::run(const SpecConstant *newSpecConstants, uint32_t newSpecConstantCount, std::vector<uint8_t> &optimizedData) const {
        assert(!empty());

        optimizedData.resize(spirvWordCount * sizeof(uint32_t));
        memcpy(optimizedData.data(), spirvWords, optimizedData.size());
        
        // Patch in specialization constants.
        uint32_t *optimizedWords = reinterpret_cast<uint32_t *>(optimizedData.data());
        for (uint32_t i = 0; i < newSpecConstantCount; i++) {
            const SpecConstant &newSpecConstant = newSpecConstants[i];
            if (newSpecConstant.specId >= specIdToConstantIndex.size()) {
                fprintf(stderr, "Optimization error. Spec Id %u was not detected in the shader.\n", newSpecConstant.specId);
                return false;
            }

            uint32_t specIndex = specIdToConstantIndex[newSpecConstant.specId];
            if (specIndex == UINT32_MAX) {
                fprintf(stderr, "Optimization error. Spec Id %u was not detected in the shader.\n", newSpecConstant.specId);
                return false;
            }

            const SpecConstant &specConstant = specConstants[specIndex];
            if (specConstant.values.size() != newSpecConstant.values.size()) {
                fprintf(stderr, "Optimization error. Spec Id %u expects %zu values but %zu were provided. The value count must be the exact same.\n", newSpecConstant.specId, specConstant.values.size(), newSpecConstant.values.size());
                return false;
            }

            uint32_t targetId = specConstantsTargetIds[specIndex];
            uint32_t specWordIndex = resultIndices[targetId];
            SpvOp specOpCode = SpvOp(spirvWords[specWordIndex] & 0xFFFFU);
            switch (specOpCode) {
            case SpvOpSpecConstantTrue:
            case SpvOpSpecConstantFalse:
                optimizedWords[specWordIndex] = (newSpecConstant.values[0] ? SpvOpConstantTrue : SpvOpConstantFalse) | (optimizedWords[specWordIndex] & 0xFFFF0000U);
                break;
            case SpvOpSpecConstant:
                optimizedWords[specWordIndex] = SpvOpConstant | (optimizedWords[specWordIndex] & 0xFFFF0000U);
                memcpy(&optimizedWords[specWordIndex + 3], newSpecConstant.values.data(), sizeof(uint32_t) * specConstant.values.size());
                break;
            default:
                fprintf(stderr, "Optimization error. Can't patch opCode %u.\n", specOpCode);
                return false;
            }
        }

        return true;
    }
};