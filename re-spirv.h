//
// re-spirv
//
// Copyright (c) 2024 renderbag and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE file for details.
//

#pragma once

#include <memory>
#include <vector>

namespace respv {
    struct SpecConstant {
        uint32_t specId = 0;
        std::vector<uint32_t> values;

        SpecConstant() {
            // Empty constructor.
        }

        SpecConstant(uint32_t pSpecId, const std::vector<uint32_t> &pValues) {
            specId = pSpecId;
            values = pValues;
        }
    };

    struct Instruction {
        uint32_t wordIndex = UINT32_MAX;
        uint32_t adjacentListIndex = UINT32_MAX;

        Instruction(uint32_t pWordIndex) {
            wordIndex = pWordIndex;
        }
    };

    struct Result {
        uint32_t instructionIndex = UINT32_MAX;

        Result() {
            // Empty.
        }

        Result(uint32_t pInstructionIndex) {
            instructionIndex = pInstructionIndex;
        }
    };

    struct Specialization {
        uint32_t constantInstructionIndex = UINT32_MAX;
        uint32_t decorationInstructionIndex = UINT32_MAX;

        Specialization() {
            // Empty.
        }

        Specialization(uint32_t pConstantInstructionIndex, uint32_t pDecorationInstructionIndex) {
            constantInstructionIndex = pConstantInstructionIndex;
            decorationInstructionIndex = pDecorationInstructionIndex;
        }
    };

    struct Decoration {
        uint32_t instructionIndex = UINT32_MAX;

        Decoration() {
            // Empty.
        }

        Decoration(uint32_t pInstructionIndex) {
            instructionIndex = pInstructionIndex;
        }
    };

    struct Phi {
        uint32_t instructionIndex = UINT32_MAX;

        Phi() {
            // Empty.
        }

        Phi(uint32_t pInstructionIndex) {
            instructionIndex = pInstructionIndex;
        }
    };

    struct ListNode {
        uint32_t instructionIndex = UINT32_MAX;
        uint32_t nextListIndex = UINT32_MAX;

        ListNode(uint32_t pInstructionIndex, uint32_t pNextListIndex) {
            instructionIndex = pInstructionIndex;
            nextListIndex = pNextListIndex;
        }
    };

    struct Shader {
        const uint32_t *spirvWords = nullptr;
        size_t spirvWordCount = 0;
        std::vector<Instruction> instructions;
        std::vector<uint32_t> instructionInDegrees;
        std::vector<uint32_t> instructionOutDegrees;
        std::vector<uint32_t> instructionOrder;
        std::vector<Result> results;
        std::vector<Specialization> specializations;
        std::vector<Decoration> decorations;
        std::vector<Phi> phis;
        std::vector<ListNode> listNodes;
        uint32_t defaultSwitchOpConstantInt = UINT32_MAX;

        Shader();
        Shader(const void *pData, size_t pSize);
        void clear();
        uint32_t addToList(uint32_t pInstructionIndex, uint32_t pListIndex);
        bool parseWords(const void *pData, size_t pSize);
        bool parse(const void *pData, size_t pSize);
        bool process();
        bool sort();
        bool empty() const;
    };

    struct Options {
        bool removeDeadCode = true;
    };

    struct Optimizer {
        static bool run(const Shader &pShader, const SpecConstant *pNewSpecConstants, uint32_t pNewSpecConstantCount, std::vector<uint8_t> &pOptimizedData, Options pOptions = Options());
    };
};