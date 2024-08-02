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

        Instruction() {
            // Empty constructor.
        }

        Instruction(uint32_t wordIndex) {
            this->wordIndex = wordIndex;
        }
    };

    struct Result {
        uint32_t instructionIndex = UINT32_MAX;
        uint32_t wordIndex = UINT32_MAX;
        uint32_t adjacentListIndex = UINT32_MAX;

        Result() {
            // Empty constructor.
        }

        Result(uint32_t wordIndex) {
            this->wordIndex = wordIndex;
        }
    };

    struct Decorator {
        uint32_t wordIndex = UINT32_MAX;

        Decorator(uint32_t wordIndex) {
            this->wordIndex = wordIndex;
        }
    };

    struct Block {
        uint32_t labelIndex = UINT32_MAX;
        uint32_t terminationIndex = UINT32_MAX;
    };

    enum class IndexType {
        None,
        Result,
        Instruction
    };

    struct ListNode {
        uint32_t index = UINT32_MAX;
        IndexType indexType = IndexType::None;
        uint32_t nextListIndex = UINT32_MAX;

        ListNode() {
            // Empty constructor.
        }

        ListNode(uint32_t index, IndexType indexType, uint32_t nextListIndex) {
            this->index = index;
            this->indexType = indexType;
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
        std::vector<ListNode> listNodes;
        bool valid = false;

        bool parse(const void *data, size_t size);
        bool empty() const;
    };

    struct Optimizer {
        Shader shader;

        // Default empty constructor.
        Optimizer();

        // Parses the SPIR-V during construction. Data must remain available during the lifetime of the object.
        Optimizer(const void *data, size_t size);

        // Data must remain available during the lifetime of the object.
        bool parse(const void *data, size_t size);

        // Check whether the optimizer has parsed any SPIR-V data.
        bool empty() const;

        // Return the specialization constants that were parsed.
        const std::vector<SpecConstant> &getSpecConstants() const;

        // Run the optimizer with the specified values for specialization constants and return the optimized data.
        bool run(const SpecConstant *newSpecConstants, uint32_t newSpecConstantCount, std::vector<uint8_t> &optimizedData) const;
    };

    struct Debugger {
        static void printTraversalFrom(const Shader &shader, uint32_t resultId);
    };
};