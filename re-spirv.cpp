//
// re-spirv
//

#include "re-spirv.h"

#include <cassert>
#include <stack>

#define SPV_ENABLE_UTILITY_CODE

#include "spirv/unified1/spirv.h"

// TODO: Replace stderr handling with a consumer printer class.

namespace respv {
    // Common.

    static bool SpvHasOperandRange(SpvOp opCode, uint32_t &operandWordStart, uint32_t &operandWordCount) {
        switch (opCode) {
        case SpvOpSelect:
            operandWordStart = 3;
            operandWordCount = 3;
            return true;
        case SpvOpNot:
        case SpvOpBitcast:
            operandWordStart = 3;
            operandWordCount = 1;
            return true;
        case SpvOpIEqual:
        case SpvOpINotEqual:
        case SpvOpUGreaterThan:
        case SpvOpSGreaterThan:
        case SpvOpUGreaterThanEqual:
        case SpvOpSGreaterThanEqual:
        case SpvOpULessThan:
        case SpvOpSLessThan:
        case SpvOpULessThanEqual:
        case SpvOpSLessThanEqual:
        case SpvOpShiftRightLogical:
        case SpvOpShiftRightArithmetic:
        case SpvOpShiftLeftLogical:
        case SpvOpBitwiseOr:
        case SpvOpBitwiseAnd:
        case SpvOpBitwiseXor:
            operandWordStart = 3;
            operandWordCount = 2;
            return true;
        case SpvOpPhi:
            operandWordStart = 3;
            operandWordCount = UINT32_MAX;
            return true;
        case SpvOpBranchConditional:
        case SpvOpSwitch:
            operandWordStart = 1;
            operandWordCount = 1;
            return true;
        default:
            operandWordStart = operandWordCount = 0;
            return false;
        }
    }

    static bool SpvHasLabels(SpvOp opCode, uint32_t &labelWordStart, uint32_t &labelWordCount, uint32_t &labelWordStride) {
        switch (opCode) {
        case SpvOpBranch:
            labelWordStart = 1;
            labelWordCount = 1;
            labelWordStride = 1;
            return true;
        case SpvOpBranchConditional:
            labelWordStart = 2;
            labelWordCount = 2;
            labelWordStride = 1;
            return true;
        case SpvOpSwitch:
            labelWordStart = 2;
            labelWordCount = UINT32_MAX;
            labelWordStride = 2;
            return true;
        default:
            labelWordStart = labelWordCount = labelWordStride = 0;
            return false;
        }
    }

    // Shader.

    Shader::Shader() {
        // Empty.

    }

    Shader::Shader(const void *data, size_t size) {
        parse(data, size);
    }

    void Shader::clear() {
        spirvWords = nullptr;
        spirvWordCount = 0;
        specConstants.clear();
        specConstantsTargetIds.clear();
        specIdToConstantIndex.clear();
        instructions.clear();
        results.clear();
        decorators.clear();
        blocks.clear();
        blockDegrees.clear();
        listNodes.clear();
        valid = false;
    }

    uint32_t Shader::addToList(uint32_t id, IdType idType, uint32_t listIndex) {
        listNodes.emplace_back(id, idType, listIndex);
        return uint32_t(listNodes.size() - 1);
    }
    
    bool Shader::parseWords(const void *data, size_t size) {
        assert(data != nullptr);
        assert(size > 0);

        spirvWords = reinterpret_cast<const uint32_t *>(data);
        spirvWordCount = size / sizeof(uint32_t);

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
        instructions.reserve(idBound);
        listNodes.reserve(idBound);
        results.resize(idBound, Result());
        results.shrink_to_fit();

        // Parse all instructions.
        uint32_t wordIndex = startingWordIndex;
        Block activeBlock;
        activeBlock.wordIndex = wordIndex;
        while (wordIndex < spirvWordCount) {
            uint32_t instructionIndex = uint32_t(instructions.size());
            SpvOp opCode = SpvOp(spirvWords[wordIndex] & 0xFFFFU);
            uint16_t wordCount = (spirvWords[wordIndex] >> 16U) & 0xFFFFU;

            bool hasResult, hasType;
            SpvHasResultAndType(opCode, &hasResult, &hasType);

            uint32_t resultId = 0;
            if (hasResult) {
                resultId = spirvWords[wordIndex + (hasType ? 2 : 1)];
                if (resultId >= idBound) {
                    fprintf(stderr, "SPIR-V Parsing error. Invalid Result ID: %u.\n", resultId);
                    return false;
                }

                assert(results[resultId].instructionIndex == UINT32_MAX && "Two instructions can't write to the same result.");
                results[resultId].instructionIndex = instructionIndex;
            }
            else {
                resultId = UINT32_MAX;
            }

            bool endBlock = false;
            switch (opCode) {
            case SpvOpDecorate:
                decorators.emplace_back(instructionIndex);
                break;
            case SpvOpLabel:
                // If a block is already in progress, it is only allowed if it's not labeled.
                if (activeBlock.instructionCount > 0) {
                    if (isBlockLabeled(activeBlock)) {
                        fprintf(stderr, "SPIR-V Parsing error. A block can't be started while another block is in progress.\n");
                        return false;
                    }
                    else {
                        blocks.emplace_back(activeBlock);
                    }
                }

                // Start a new labeled block.
                activeBlock.wordIndex = wordIndex;
                activeBlock.wordCount = 0;
                activeBlock.instructionIndex = instructionIndex;
                activeBlock.instructionCount = 0;
                break;
            case SpvOpBranch:
            case SpvOpBranchConditional:
            case SpvOpSwitch:
            case SpvOpReturn:
            case SpvOpReturnValue:
            case SpvOpKill:
            case SpvOpUnreachable:
                if ((activeBlock.instructionCount == 0) || !isBlockLabeled(activeBlock)) {
                    fprintf(stderr, "SPIR-V Parsing error. Encountered a termination instruction but no labeled block was in progress.\n");
                    return false;
                }
                else {
                    // Indicate the active block should be finished.
                    endBlock = true;
                }

                break;
            default:
                // Ignore the rest.
                break;
            }

            uint32_t operandWordStart, operandWordCount;
            if (SpvHasOperandRange(opCode, operandWordStart, operandWordCount)) {
                if (wordCount <= operandWordStart) {
                    fprintf(stderr, "SPIR-V Parsing error. Instruction doesn't have enough words for operand count.\n");
                    return false;
                }

                operandWordCount = std::min(uint32_t(wordCount) - operandWordStart, operandWordCount);

                for (uint32_t i = 0; i < operandWordCount; i++) {
                    uint32_t operandId = spirvWords[wordIndex + operandWordStart + i];
                    if (operandId >= idBound) {
                        fprintf(stderr, "SPIR-V Parsing error. Invalid Operand ID: %u.\n", operandId);
                        return false;
                    }

                    bool addResult = (resultId != UINT32_MAX);
                    results[operandId].adjacentListIndex = addToList(addResult ? resultId : instructionIndex, addResult ? IdType::Result : IdType::Instruction, results[operandId].adjacentListIndex);
                }
            }

            if (wordCount == 0) {
                fprintf(stderr, "Unknown SPIR-V Parsing error.\n");
                return false;
            }

            instructions.emplace_back(wordIndex, uint32_t(blocks.size()));
            activeBlock.instructionCount++;
            activeBlock.wordCount += wordCount;
            wordIndex += wordCount;

            if (endBlock) {
                blocks.emplace_back(activeBlock);
                activeBlock.wordIndex = wordIndex;
                activeBlock.wordCount = 0;
                activeBlock.instructionIndex = instructionIndex + 1;
                activeBlock.instructionCount = 0;
            }
        }

        if (activeBlock.instructionCount > 0) {
            if (isBlockLabeled(activeBlock)) {
                fprintf(stderr, "SPIR-V Parsing error. Last block of the shader was not finished.\n");
                return false;
            }
            else {
                blocks.emplace_back(activeBlock);
            }
        }

        return true;
    }

    bool Shader::isBlockLabeled(const Block &block) const {
        const Instruction &labelInstruction = instructions[block.instructionIndex];
        return (SpvOp(spirvWords[labelInstruction.wordIndex] & 0xFFFFU) == SpvOpLabel);
    }

    bool Shader::processBlockAdjacentTo(Block &block, uint32_t labelId) {
        if (results[labelId].instructionIndex == UINT32_MAX) {
            fprintf(stderr, "SPIR-V Parsing error. Label %u does not exist.\n", labelId);
            return false;
        }

        const Instruction &labelInstruction = instructions[results[labelId].instructionIndex];
        if (SpvOp(spirvWords[labelInstruction.wordIndex] & 0xFFFFU) != SpvOpLabel) {
            fprintf(stderr, "SPIR-V Parsing error. Result %u is not a label.\n", labelId);
            return false;
        }

        block.adjacentListIndex = addToList(labelInstruction.blockIndex, IdType::Block, block.adjacentListIndex);
        blockDegrees[labelInstruction.blockIndex] += 1;
        return true;
    }

    bool Shader::processBlocks() {
        blockDegrees.clear();
        blockDegrees.resize(blocks.size(), 0);
        
        bool firstLabeledBlockMaxDegree = true;
        for (uint32_t i = 0; i < blocks.size(); i++) {
            Block &block = blocks[i];
            if (!isBlockLabeled(block)) {
                // Unlabeled blocks are always included.
                blockDegrees[i] = 1;
                continue;
            }

            // We can't reach the first labeled block from anywhere, so it must be included.
            if (firstLabeledBlockMaxDegree) {
                blockDegrees[i] = 1;
                firstLabeledBlockMaxDegree = false;
            }

            uint32_t endWordIndex = instructions[block.endInstructionIndex()].wordIndex;
            SpvOp opCode = SpvOp(spirvWords[endWordIndex] & 0xFFFFU);
            uint16_t endWordCount = (spirvWords[endWordIndex] >> 16U) & 0xFFFFU;
            uint32_t labelWordStart, labelWordCount, labelWordStride;
            if (SpvHasLabels(opCode, labelWordStart, labelWordCount, labelWordStride)) {
                for (uint32_t j = 0; (j < labelWordCount) && ((labelWordStart + j * labelWordStride) < endWordCount); j++) {
                    if (!processBlockAdjacentTo(block, spirvWords[endWordIndex + labelWordStart + j * labelWordStride])) {
                        return false;
                    }
                }
            }

            if (block.instructionCount >= 3) {
                uint32_t mergeWordIndex = instructions[block.mergeInstructionIndex()].wordIndex;
                SpvOp mergeOpCode = SpvOp(spirvWords[mergeWordIndex] & 0xFFFFU);
                if (mergeOpCode == SpvOpSelectionMerge) {
                    if (!processBlockAdjacentTo(block, spirvWords[mergeWordIndex + 1])) {
                        return false;
                    }
                }
                else if (mergeOpCode == SpvOpLoopMerge) {
                    fprintf(stderr, "SPIR-V Parsing error. SpvOpLoopMerge is not supported yet.\n");
                    return false;
                }
            }
        }

        return true;
    }

    bool Shader::processDecorators() {
        std::vector<uint32_t> specValues;
        for (const Decorator &decorator : decorators) {
            uint32_t decoratorWordIndex = instructions[decorator.instructionIndex].wordIndex;
            uint32_t decoration = spirvWords[decoratorWordIndex + 2];
            switch (decoration) {
            case SpvDecorationSpecId: {
                uint32_t targetId = spirvWords[decoratorWordIndex + 1];
                const Result &specResult = results[targetId];
                if (specResult.instructionIndex == UINT32_MAX) {
                    fprintf(stderr, "SPIR-V Parsing error. SpvDecorationSpecId targets %u which hasn't been defined before it.\n", targetId);
                    return false;
                }

                uint32_t specWordIndex = instructions[specResult.instructionIndex].wordIndex;
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

                uint32_t constantId = spirvWords[decoratorWordIndex + 3];
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

        return true;
    }

    bool Shader::parse(const void *data, size_t size) {
        assert(data != nullptr);
        assert((size % sizeof(uint32_t) == 0) && "Size of data must be aligned to the word size.");

        clear();

        if (!parseWords(data, size)) {
            return false;
        }

        if (!processBlocks()) {
            return false;
        }

        if (!processDecorators()) {
            return false;
        }

        valid = true;
        return true;
    }

    bool Shader::empty() const {
        return !valid;
    }

    const Instruction Shader::resultToInstruction(uint32_t resultId) const {
        assert(results[resultId].instructionIndex < instructions.size());
        return instructions[results[resultId].instructionIndex];
    }

    uint32_t Shader::resultToWordIndex(uint32_t resultId) const {
        return resultToInstruction(resultId).wordIndex;
    }

    // Optimizer.

    struct Resolution {
        enum Type {
            Unknown,
            Constant,
            Variable
        };

        Type type = Type::Unknown;

        struct {
            union {
                int32_t i32;
                uint32_t u32;
            };
        } value = {};

        static Resolution fromBool(bool value) {
            Resolution r;
            r.type = Type::Constant;
            r.value.u32 = value ? 1 : 0;
            return r;
        }

        static Resolution fromInt32(int32_t value) {
            Resolution r;
            r.type = Type::Constant;
            r.value.i32 = value;
            return r;
        }

        static Resolution fromUint32(uint32_t value) {
            Resolution r;
            r.type = Type::Constant;
            r.value.u32 = value;
            return r;
        }
    };

    static void prepareShaderData(const Shader &shader, std::vector<uint32_t> &localBlockDegrees, std::vector<uint32_t> &localBlockReductions, std::vector<uint8_t> &optimizedData) {
        const size_t originalDataSize = shader.spirvWordCount * sizeof(uint32_t);
        optimizedData.resize(originalDataSize);
        memcpy(optimizedData.data(), shader.spirvWords, originalDataSize);

        localBlockDegrees.resize(shader.blockDegrees.size());
        memcpy(localBlockDegrees.data(), shader.blockDegrees.data(), localBlockDegrees.size() * sizeof(uint32_t));

        localBlockReductions.clear();
        localBlockReductions.resize(shader.blocks.size(), 0);
    }

    static void reduceBlockDegree(const Shader &shader, uint32_t firstBlockIndex, std::vector<uint32_t> &localBlockDegrees) {
        assert(firstBlockIndex < localBlockDegrees.size());

        thread_local std::stack<uint32_t> blockStack;
        blockStack.emplace(firstBlockIndex);

        while (!blockStack.empty()) {
            uint32_t blockIndex = blockStack.top();
            blockStack.pop();

            assert((localBlockDegrees[blockIndex] > 0) && "It should not be possible for a block's degree to be reduced below 0.");
            localBlockDegrees[blockIndex]--;

            // When a block's degree reaches zero, all adjacent blocks must also be decreased.
            if (localBlockDegrees[blockIndex] == 0) {
                uint32_t listIndex = shader.blocks[blockIndex].adjacentListIndex;
                while (listIndex != UINT32_MAX) {
                    const ListNode &listNode = shader.listNodes[listIndex];
                    assert((listNode.idType == IdType::Block) && "Only blocks must exist in the adjacency list");
                    blockStack.push(listNode.id);
                    listIndex = listNode.nextListIndex;
                }
            }
        }
    }

    static void reduceBlockDegreeByLabel(const Shader &shader, uint32_t resultId, std::vector<uint32_t> &localBlockDegrees) {
        const Instruction &instruction = shader.resultToInstruction(resultId);
        reduceBlockDegree(shader, instruction.blockIndex, localBlockDegrees);
    }

    static void solveResult(const Shader &shader, uint32_t resultId, std::vector<Resolution> &resolutions, const std::vector<uint8_t> &optimizedData) {
        // This function assumes all operands have already been evaluated by the caller and are constant.
        const uint32_t *optimizedWords = reinterpret_cast<const uint32_t *>(optimizedData.data());
        const Result &result = shader.results[resultId];
        Resolution &resolution = resolutions[resultId];
        uint32_t resultWordIndex = shader.instructions[result.instructionIndex].wordIndex;
        SpvOp opCode = SpvOp(optimizedWords[resultWordIndex] & 0xFFFFU);
        uint16_t wordCount = (optimizedWords[resultWordIndex] >> 16U) & 0xFFFFU;
        switch (opCode) {
        case SpvOpConstant: {
            // Parse the known type of constants. Any other types will be considered as variable.
            uint32_t typeWordIndex = shader.resultToWordIndex(optimizedWords[resultWordIndex + 1]);
            SpvOp typeOpCode = SpvOp(optimizedWords[typeWordIndex] & 0xFFFFU);
            uint32_t typeWidthInBits = optimizedWords[typeWordIndex + 2];
            uint32_t typeSigned = optimizedWords[typeWordIndex + 3];
            if ((typeOpCode == SpvOpTypeInt) && (typeWidthInBits == 32)) {
                if (typeSigned) {
                    resolution = Resolution::fromInt32(int32_t(optimizedWords[resultWordIndex + 3]));
                }
                else {
                    resolution = Resolution::fromUint32(optimizedWords[resultWordIndex + 3]);
                }
            }
            else {
                resolution.type = Resolution::Type::Variable;
            }

            break;
        }
        case SpvOpConstantTrue:
            resolution = Resolution::fromBool(true);
            break;
        case SpvOpConstantFalse:
            resolution = Resolution::fromBool(false);
            break;
        case SpvOpShiftRightLogical: {
            const Resolution &baseResolution = resolutions[optimizedWords[resultWordIndex + 3]];
            const Resolution &shiftResolution = resolutions[optimizedWords[resultWordIndex + 4]];
            resolution = Resolution::fromUint32(baseResolution.value.u32 << shiftResolution.value.u32);
            break;
        }
        case SpvOpShiftRightArithmetic: {
            const Resolution &baseResolution = resolutions[optimizedWords[resultWordIndex + 3]];
            const Resolution &shiftResolution = resolutions[optimizedWords[resultWordIndex + 4]];
            resolution = Resolution::fromInt32(baseResolution.value.i32 << shiftResolution.value.i32);
            break;
        }
        case SpvOpShiftLeftLogical: {
            const Resolution &baseResolution = resolutions[optimizedWords[resultWordIndex + 3]];
            const Resolution &shiftResolution = resolutions[optimizedWords[resultWordIndex + 4]];
            resolution = Resolution::fromUint32(baseResolution.value.u32 >> shiftResolution.value.u32);
            break;
        }
        case SpvOpBitwiseOr: {
            const Resolution &firstResolution = resolutions[optimizedWords[resultWordIndex + 3]];
            const Resolution &secondResolution = resolutions[optimizedWords[resultWordIndex + 4]];
            resolution = Resolution::fromUint32(firstResolution.value.u32 | secondResolution.value.u32);
            break;
        }
        case SpvOpBitwiseAnd: {
            const Resolution &firstResolution = resolutions[optimizedWords[resultWordIndex + 3]];
            const Resolution &secondResolution = resolutions[optimizedWords[resultWordIndex + 4]];
            resolution = Resolution::fromUint32(firstResolution.value.u32 & secondResolution.value.u32);
            break;
        }
        case SpvOpBitwiseXor: {
            const Resolution &firstResolution = resolutions[optimizedWords[resultWordIndex + 3]];
            const Resolution &secondResolution = resolutions[optimizedWords[resultWordIndex + 4]];
            resolution = Resolution::fromUint32(firstResolution.value.u32 ^ secondResolution.value.u32);
            break;
        }
        default:
            // It's not known how to evaluate the instruction, consider the result a variable.
            resolution.type = Resolution::Type::Variable;
            break;
        }
    }

    static void evaluateResult(const Shader &shader, uint32_t firstResultId, std::vector<Resolution> &resolutions, const std::vector<uint8_t> &optimizedData) {
        thread_local std::stack<uint32_t> resultStack;
        resultStack.emplace(firstResultId);

        const uint32_t *optimizedWords = reinterpret_cast<const uint32_t *>(optimizedData.data());
        while (!resultStack.empty()) {
            uint32_t resultId = resultStack.top();
            resultStack.pop();

            Resolution &resolution = resolutions[resultId];
            if (resolution.type != Resolution::Type::Unknown) {
                continue;
            }

            // If the result has a known operation range and the optimizer knows how to resolve it, check if the operators are solved.
            // If some of the operators are not solved, push this result again to the stack and the unknown operators afterwards so they're resolved first.
            // If any of the operators is resolved into a variable, then the result is not evaluated and is considered a variable as well.
            // If all the operators are constant, then the operation is resolved from the values of the resolutions.
            // If the result doesn't have a known operator range, just attempt to solve the result directly.
            const Result &result = shader.results[resultId];
            uint32_t resultWordIndex = shader.instructions[result.instructionIndex].wordIndex;
            SpvOp opCode = SpvOp(optimizedWords[resultWordIndex] & 0xFFFFU);
            uint16_t resultWordCount = (optimizedWords[resultWordIndex] >> 16U) & 0xFFFFU;
            uint32_t operandWordStart, operandWordCount;
            if (SpvHasOperandRange(opCode, operandWordStart, operandWordCount)) {
                bool returnedToStack = false;
                for (uint32_t i = 0; (i < operandWordCount) && ((operandWordStart + i) < resultWordCount); i++) {
                    uint32_t operandResultId = optimizedWords[resultWordIndex + operandWordStart + i];
                    if (resolutions[operandResultId].type == Resolution::Type::Variable) {
                        resolution.type = Resolution::Type::Variable;
                        break;
                    }
                    else if (resolutions[operandResultId].type == Resolution::Type::Unknown) {
                        if (!returnedToStack) {
                            resultStack.push(resultId);
                            returnedToStack = true;
                        }

                        resultStack.push(operandResultId);
                    }
                }

                // All operators are known to be constant. Attempt to evaluate this instruction directly.
                if (!returnedToStack && (resolution.type == Resolution::Type::Unknown)) {
                    solveResult(shader, resultId, resolutions, optimizedData);
                }
            }
            else {
                solveResult(shader, resultId, resolutions, optimizedData);
            }
        }
    }

    static void evaluateTerminator(const Shader &shader, uint32_t instructionIndex, std::vector<Resolution> &resolutions, std::vector<uint32_t> &localBlockDegrees, std::vector<uint32_t> &localBlockReductions, std::vector<uint8_t> &optimizedData) {
        // For each type of supported terminator, check if the operands can be resolved into constants.
        // If they can be resolved, eliminate any other branches that don't pass the condition.
        uint32_t *optimizedWords = reinterpret_cast<uint32_t *>(optimizedData.data());
        const Instruction &instruction = shader.instructions[instructionIndex];
        uint32_t wordIndex = instruction.wordIndex;
        SpvOp opCode = SpvOp(optimizedWords[wordIndex] & 0xFFFFU);
        uint16_t wordCount = (optimizedWords[wordIndex] >> 16U) & 0xFFFFU;
        uint32_t finalBranchLabelId = UINT32_MAX;
        if ((opCode == SpvOpBranchConditional) || (opCode == SpvOpSwitch)) {
            // Both instructions share that the second word is the operator they must use to resolve the condition.
            const uint32_t operatorId = optimizedWords[wordIndex + 1];
            evaluateResult(shader, operatorId, resolutions, optimizedData);

            // Operator can't be anything but a constant to be able to resolve a terminator.
            const Resolution &operatorResolution = resolutions[operatorId];
            if (operatorResolution.type != Resolution::Type::Constant) {
                return;
            }

            if (opCode == SpvOpBranchConditional) {
                // Branch conditional only needs to choose either label depending on whether the result is true or false.
                if (operatorResolution.value.u32) {
                    finalBranchLabelId = optimizedWords[wordIndex + 2];
                    reduceBlockDegreeByLabel(shader, optimizedWords[wordIndex + 3], localBlockDegrees);
                }
                else {
                    finalBranchLabelId = optimizedWords[wordIndex + 3];
                    reduceBlockDegreeByLabel(shader, optimizedWords[wordIndex + 2], localBlockDegrees);
                }
            }
            else if (opCode == SpvOpSwitch) {
                // Switch must compare the integer result of the operator to all the possible labels.
                // If the label is not as possible result, then reduce its block's degree.
                for (uint32_t i = 3; i < wordCount; i += 2) {
                    if (operatorResolution.value.u32 == optimizedWords[wordIndex + i]) {
                        finalBranchLabelId = optimizedWords[wordIndex + i];
                    }
                    else {
                        reduceBlockDegreeByLabel(shader, optimizedWords[wordIndex + i + 1], localBlockDegrees);
                    }
                }

                // If none are chosen, the default label is selected. Otherwise, reduce the block's degree
                // for the default label.
                if (finalBranchLabelId == UINT32_MAX) {
                    finalBranchLabelId = optimizedWords[wordIndex + 2];
                }
                else {
                    reduceBlockDegreeByLabel(shader, optimizedWords[wordIndex + 2], localBlockDegrees);
                }
            }
            
            // Patch the terminator to be an unconditional branch. Reduce the block's size.
            if (finalBranchLabelId != UINT32_MAX) {
                optimizedWords[wordIndex] = SpvOpBranch | (2U << 16U);
                optimizedWords[wordIndex + 1] = finalBranchLabelId;
                localBlockReductions[instruction.blockIndex] = (wordCount - 2) * sizeof(uint32_t);
            }
        }
    }

    static bool runOptimizationPassFrom(const Shader &shader, uint32_t firstResultId, std::vector<Resolution> &resolutions, std::vector<uint32_t> &localBlockDegrees, std::vector<uint32_t> &localBlockReductions, std::vector<uint8_t> &optimizedData) {
        // Do a traversal by looking at the adjacency list of the result.
        thread_local std::stack<uint32_t> resultStack;
        resultStack.emplace(firstResultId);

        while (!resultStack.empty()) {
            uint32_t resultId = resultStack.top();
            resultStack.pop();
            evaluateResult(shader, resultId, resolutions, optimizedData);

            // We do not need to explore this path further if it can't be resolved into a constant.
            if (resolutions[resultId].type != Resolution::Type::Constant) {
                continue;
            }

            uint32_t listIndex = shader.results[resultId].adjacentListIndex;
            while (listIndex != UINT32_MAX) {
                const ListNode &listNode = shader.listNodes[listIndex];
                if (listNode.idType == IdType::Result) {
                    resultStack.push(listNode.id);
                }
                else if (listNode.idType == IdType::Instruction) {
                    evaluateTerminator(shader, listNode.id, resolutions, localBlockDegrees, localBlockReductions, optimizedData);
                }
                else {
                    assert(false && "No other types of Ids should exist in the adjacency list.");
                }

                listIndex = listNode.nextListIndex;
            }
        }

        return true;
    }

    static bool optimizeSpecializationConstants(const Shader &shader, const SpecConstant *newSpecConstants, uint32_t newSpecConstantCount, std::vector<uint32_t> &localBlockDegrees, std::vector<uint32_t> &localBlockReductions, std::vector<uint8_t> &optimizedData) {
        // Allocate the resolutions vector that will be shared between all optimization passes.
        thread_local std::vector<Resolution> resolutions;
        resolutions.clear();
        resolutions.resize(shader.results.size(), Resolution());

        // Run the optimization pass from every specialization constant that needs to be patched.
        uint32_t *optimizedWords = reinterpret_cast<uint32_t *>(optimizedData.data());
        for (uint32_t i = 0; i < newSpecConstantCount; i++) {
            const SpecConstant &newSpecConstant = newSpecConstants[i];
            if (newSpecConstant.specId >= shader.specIdToConstantIndex.size()) {
                fprintf(stderr, "Optimization error. Spec Id %u was not detected in the shader.\n", newSpecConstant.specId);
                return false;
            }

            uint32_t specIndex = shader.specIdToConstantIndex[newSpecConstant.specId];
            if (specIndex == UINT32_MAX) {
                fprintf(stderr, "Optimization error. Spec Id %u was not detected in the shader.\n", newSpecConstant.specId);
                return false;
            }

            const SpecConstant &specConstant = shader.specConstants[specIndex];
            if (specConstant.values.size() != newSpecConstant.values.size()) {
                fprintf(stderr, "Optimization error. Spec Id %u expects %zu values but %zu were provided. The value count must be the exact same.\n", newSpecConstant.specId, specConstant.values.size(), newSpecConstant.values.size());
                return false;
            }

            uint32_t resultId = shader.specConstantsTargetIds[specIndex];
            const Result &specResult = shader.results[resultId];
            uint32_t specWordIndex = shader.instructions[specResult.instructionIndex].wordIndex;
            SpvOp specOpCode = SpvOp(shader.spirvWords[specWordIndex] & 0xFFFFU);
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

            if (!runOptimizationPassFrom(shader, resultId, resolutions, localBlockDegrees, localBlockReductions, optimizedData)) {
                return false;
            }
        }

        return true;
    }

    static void compactShader(const Shader &shader, const std::vector<uint32_t> &localBlockDegrees, const std::vector<uint32_t> &localBlockReductions, std::vector<uint8_t> &optimizedData) {
        // Ignore the header.
        size_t optimizedDataSize = 0;
        const size_t headerSize = 5 * sizeof(uint32_t);
        optimizedDataSize += headerSize;

        // Perform compaction of the data by moving the blocks if necessary.
        for (uint32_t i = 0; i < uint32_t(shader.blocks.size()); i++) {
            if (localBlockDegrees[i] == 0) {
                continue;
            }

            const size_t originalPosition = shader.blocks[i].wordIndex * sizeof(uint32_t);
            size_t blockSize = shader.blocks[i].wordCount * sizeof(uint32_t);
            assert(localBlockReductions[i] <= blockSize && "Local block reduction can't be bigger than the block's size.");
            blockSize -= localBlockReductions[i];

            if ((blockSize > 0) && (optimizedDataSize != originalPosition)) {
                memmove(&optimizedData[optimizedDataSize], &optimizedData[originalPosition], blockSize);
            }

            optimizedDataSize += blockSize;
        }

        optimizedData.resize(optimizedDataSize);
    }

    bool Optimizer::run(const Shader &shader, const SpecConstant *newSpecConstants, uint32_t newSpecConstantCount, std::vector<uint8_t> &optimizedData) {
        if (shader.empty()) {
            fprintf(stderr, "Optimization error. Shader is empty.\n");
            return false;
        }

        thread_local std::vector<uint32_t> localBlockDegrees;
        thread_local std::vector<uint32_t> localBlockReductions;
        prepareShaderData(shader, localBlockDegrees, localBlockReductions, optimizedData);

        if (!optimizeSpecializationConstants(shader, newSpecConstants, newSpecConstantCount, localBlockDegrees, localBlockReductions, optimizedData)) {
            return false;
        }

        compactShader(shader, localBlockDegrees, localBlockReductions, optimizedData);
        return true;
    }

    // Debugger.

    struct Iteration {
        uint32_t id = UINT32_MAX;
        IdType idType = IdType::None;
        uint32_t depth = 0;

        Iteration() {
            // Empty constructor.
        }

        Iteration(uint32_t id, IdType idType, uint32_t depth) {
            this->id = id;
            this->idType = idType;
            this->depth = depth;
        }
    };
    
    void Debugger::printTraversalFrom(const Shader &shader, uint32_t resultId) {
        std::stack<Iteration> iterationStack;
        iterationStack.emplace(resultId, IdType::Result, 0);
        while (!iterationStack.empty()) {
            Iteration it = iterationStack.top();
            iterationStack.pop();

            for (uint32_t i = 0; i < it.depth; i++) {
                fprintf(stdout, "  ");
            }

            if (it.idType == IdType::Result) {
                const Result &result = shader.results[it.id];
                uint32_t resultWordIndex = shader.instructions[result.instructionIndex].wordIndex;
                SpvOp opCode = SpvOp(shader.spirvWords[resultWordIndex] & 0xFFFFU);
                fprintf(stdout, "[%d] %%%d = %s\n", result.instructionIndex, it.id, SpvOpToString(opCode));

                uint32_t listIndex = result.adjacentListIndex;
                while (listIndex != UINT32_MAX) {
                    const ListNode &listNode = shader.listNodes[listIndex];
                    iterationStack.emplace(listNode.id, listNode.idType, it.depth + 1);
                    listIndex = listNode.nextListIndex;
                }
            }
            else if (it.idType == IdType::Instruction) {
                const Instruction &instruction = shader.instructions[it.id];
                SpvOp opCode = SpvOp(shader.spirvWords[instruction.wordIndex] & 0xFFFFU);
                fprintf(stdout, "[%d] %s\n", it.id, SpvOpToString(opCode));
            }
        }
    }

    void Debugger::printBlockStatistics(const Shader &shader) {
        for (uint32_t i = 0; i < uint32_t(shader.blocks.size()); i++) {
            fprintf(stdout, "[%d] [%d] Degree %d\n", shader.blocks[i].instructionIndex, shader.blocks[i].endInstructionIndex(), shader.blockDegrees[i]);
        }
    }
};