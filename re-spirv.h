//
// re-spirv
//

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
        uint32_t blockIndex = UINT32_MAX;

        Instruction() {
            // Empty constructor.
        }

        Instruction(uint32_t wordIndex, uint32_t blockIndex) {
            this->wordIndex = wordIndex;
            this->blockIndex = blockIndex;
        }
    };

    struct Result {
        uint32_t instructionIndex = UINT32_MAX;
        uint32_t adjacentListIndex = UINT32_MAX;

        Result() {
            // Empty constructor.
        }

        Result(uint32_t instructionIndex) {
            this->instructionIndex = instructionIndex;
        }
    };

    struct Decorator {
        uint32_t instructionIndex = UINT32_MAX;

        Decorator(uint32_t instructionIndex) {
            this->instructionIndex = instructionIndex;
        }
    };

    struct Block {
        uint32_t wordIndex = 0;
        uint32_t wordCount = 0;
        uint32_t instructionIndex = 0;
        uint32_t instructionCount = 0;
        uint32_t adjacentListIndex = UINT32_MAX;

        uint32_t mergeInstructionIndex() const {
            return instructionIndex + instructionCount - 2;
        }

        uint32_t endInstructionIndex() const {
            return instructionIndex + instructionCount - 1;
        }
    };

    enum class IdType {
        None,
        Block,
        Result,
        Instruction
    };

    struct ListNode {
        uint32_t id = UINT32_MAX;
        IdType idType = IdType::None;
        uint32_t nextListIndex = UINT32_MAX;

        ListNode() {
            // Empty constructor.
        }

        ListNode(uint32_t id, IdType idType, uint32_t nextListIndex) {
            this->id = id;
            this->idType = idType;
            this->nextListIndex = nextListIndex;
        }
    };

    struct Shader {
        const uint32_t *spirvWords = nullptr;
        size_t spirvWordCount = 0;
        std::vector<SpecConstant> specConstants;
        std::vector<uint32_t> specConstantsTargetIds;
        std::vector<uint32_t> specIdToConstantIndex;
        std::vector<Instruction> instructions;
        std::vector<Result> results;
        std::vector<Decorator> decorators;
        std::vector<Block> blocks;
        std::vector<uint32_t> blockDegrees;
        std::vector<ListNode> listNodes;
        bool valid = false;

        Shader();
        Shader(const void *data, size_t size);
        void clear();
        uint32_t addToList(uint32_t id, IdType idType, uint32_t listIndex);
        bool parseWords(const void *data, size_t size);
        bool isBlockLabeled(const Block &block) const;
        bool processBlockAdjacentTo(Block &block, uint32_t labelId);
        bool processBlocks();
        bool processDecorators();
        bool parse(const void *data, size_t size);
        bool empty() const;
        const Instruction resultToInstruction(uint32_t resultId) const;
        uint32_t resultToWordIndex(uint32_t resultId) const;
    };

    struct Optimizer {
        static bool run(const Shader &shader, const SpecConstant *newSpecConstants, uint32_t newSpecConstantCount, std::vector<uint8_t> &optimizedData);
    };

    struct Debugger {
        static void printTraversalFrom(const Shader &shader, uint32_t resultId);
        static void printBlockStatistics(const Shader &shader);
    };
};