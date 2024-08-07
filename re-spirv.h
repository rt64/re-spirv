//
// re-spirv
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

        SpecConstant(uint32_t specId, const std::vector<uint32_t> &values) {
            this->specId = specId;
            this->values = values;
        }
    };

    struct Instruction {
        uint32_t wordIndex = UINT32_MAX;
        uint32_t adjacentListIndex = UINT32_MAX;

        Instruction(uint32_t wordIndex) {
            this->wordIndex = wordIndex;
        }
    };

    struct Result {
        uint32_t instructionIndex = UINT32_MAX;

        Result() {
            // Empty.
        }

        Result(uint32_t instructionIndex) {
            this->instructionIndex = instructionIndex;
        }
    };

    struct Specialization {
        uint32_t constantInstructionIndex = UINT32_MAX;
        uint32_t decorationInstructionIndex = UINT32_MAX;

        Specialization() {
            // Empty.
        }

        Specialization(uint32_t constantInstructionIndex, uint32_t decorationInstructionIndex) {
            this->constantInstructionIndex = constantInstructionIndex;
            this->decorationInstructionIndex = decorationInstructionIndex;
        }
    };

    struct Decoration {
        uint32_t instructionIndex = UINT32_MAX;

        Decoration() {
            // Empty.
        }

        Decoration(uint32_t instructionIndex) {
            this->instructionIndex = instructionIndex;
        }
    };

    struct Phi {
        uint32_t instructionIndex = UINT32_MAX;

        Phi() {
            // Empty.
        }

        Phi(uint32_t instructionIndex) {
            this->instructionIndex = instructionIndex;
        }
    };

    struct ListNode {
        uint32_t instructionIndex = UINT32_MAX;
        uint32_t nextListIndex = UINT32_MAX;

        ListNode(uint32_t instructionIndex, uint32_t nextListIndex) {
            this->instructionIndex = instructionIndex;
            this->nextListIndex = nextListIndex;
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
        Shader(const void *data, size_t size);
        void clear();
        uint32_t addToList(uint32_t instructionIndex, uint32_t listIndex);
        bool parseWords(const void *data, size_t size);
        bool parse(const void *data, size_t size);
        bool process();
        bool sort();
        bool empty() const;
    };

    struct Optimizer {
        static bool run(const Shader &shader, const SpecConstant *newSpecConstants, uint32_t newSpecConstantCount, std::vector<uint8_t> &optimizedData);
    };
};