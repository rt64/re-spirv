//
// re-spirv
//
// Copyright (c) 2024 renderbag and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE file for details.
//

#include "re-spirv.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <unordered_map>

#define SPV_ENABLE_UTILITY_CODE

#include "spirv/unified1/spirv.h"

namespace respv {
    // Common.
    
    static bool SpvIsSupported(SpvOp pOpCode) {
        switch (pOpCode) {
        case SpvOpUndef:
        case SpvOpSource:
        case SpvOpName:
        case SpvOpMemberName:
        case SpvOpExtension:
        case SpvOpExtInstImport:
        case SpvOpExtInst:
        case SpvOpMemoryModel:
        case SpvOpEntryPoint:
        case SpvOpExecutionMode:
        case SpvOpCapability:
        case SpvOpTypeVoid:
        case SpvOpTypeBool:
        case SpvOpTypeInt:
        case SpvOpTypeFloat:
        case SpvOpTypeVector:
        case SpvOpTypeMatrix:
        case SpvOpTypeImage:
        case SpvOpTypeSampler:
        case SpvOpTypeSampledImage:
        case SpvOpTypeArray:
        case SpvOpTypeRuntimeArray:
        case SpvOpTypeStruct:
        case SpvOpTypePointer:
        case SpvOpTypeFunction:
        case SpvOpConstantTrue:
        case SpvOpConstantFalse:
        case SpvOpConstant:
        case SpvOpConstantComposite:
        case SpvOpConstantNull:
        case SpvOpSpecConstant:
        case SpvOpFunction:
        case SpvOpFunctionParameter:
        case SpvOpFunctionEnd:
        case SpvOpFunctionCall:
        case SpvOpVariable:
        case SpvOpLoad:
        case SpvOpStore:
        case SpvOpAccessChain:
        case SpvOpDecorate:
        case SpvOpMemberDecorate:
        case SpvOpVectorShuffle:
        case SpvOpCompositeConstruct:
        case SpvOpCompositeExtract:
        case SpvOpCompositeInsert:
        case SpvOpCopyObject:
        case SpvOpTranspose:
        case SpvOpSampledImage:
        case SpvOpImageSampleImplicitLod:
        case SpvOpImageSampleExplicitLod:
        case SpvOpImageSampleDrefImplicitLod:
        case SpvOpImageSampleDrefExplicitLod:
        case SpvOpImageSampleProjImplicitLod:
        case SpvOpImageSampleProjExplicitLod:
        case SpvOpImageSampleProjDrefImplicitLod:
        case SpvOpImageSampleProjDrefExplicitLod:
        case SpvOpImageFetch:
        case SpvOpImageGather:
        case SpvOpImageDrefGather:
        case SpvOpImageRead:
        case SpvOpImageWrite:
        case SpvOpImage:
        case SpvOpImageQuerySizeLod:
        case SpvOpImageQueryLevels:
        case SpvOpConvertFToU:
        case SpvOpConvertFToS:
        case SpvOpConvertSToF:
        case SpvOpConvertUToF:
        case SpvOpBitcast:
        case SpvOpSNegate:
        case SpvOpFNegate:
        case SpvOpIAdd:
        case SpvOpFAdd:
        case SpvOpISub:
        case SpvOpFSub:
        case SpvOpIMul:
        case SpvOpFMul:
        case SpvOpUDiv:
        case SpvOpSDiv:
        case SpvOpFDiv:
        case SpvOpUMod:
        case SpvOpSRem:
        case SpvOpSMod:
        case SpvOpFRem:
        case SpvOpFMod:
        case SpvOpVectorTimesScalar:
        case SpvOpMatrixTimesScalar:
        case SpvOpVectorTimesMatrix:
        case SpvOpMatrixTimesVector:
        case SpvOpMatrixTimesMatrix:
        case SpvOpOuterProduct:
        case SpvOpDot:
        case SpvOpIAddCarry:
        case SpvOpISubBorrow:
        case SpvOpUMulExtended:
        case SpvOpSMulExtended:
        case SpvOpAny:
        case SpvOpAll:
        case SpvOpLogicalEqual:
        case SpvOpLogicalNotEqual:
        case SpvOpLogicalOr:
        case SpvOpLogicalAnd:
        case SpvOpLogicalNot:
        case SpvOpSelect:
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
        case SpvOpFOrdEqual:
        case SpvOpFUnordEqual:
        case SpvOpFOrdNotEqual:
        case SpvOpFUnordNotEqual:
        case SpvOpFOrdLessThan:
        case SpvOpFUnordLessThan:
        case SpvOpFOrdGreaterThan:
        case SpvOpFUnordGreaterThan:
        case SpvOpFOrdLessThanEqual:
        case SpvOpFUnordLessThanEqual:
        case SpvOpFOrdGreaterThanEqual:
        case SpvOpFUnordGreaterThanEqual:
        case SpvOpShiftRightLogical:
        case SpvOpShiftRightArithmetic:
        case SpvOpShiftLeftLogical:
        case SpvOpBitwiseOr:
        case SpvOpBitwiseXor:
        case SpvOpBitwiseAnd:
        case SpvOpNot:
        case SpvOpBitFieldInsert:
        case SpvOpBitFieldSExtract:
        case SpvOpBitFieldUExtract:
        case SpvOpBitReverse:
        case SpvOpBitCount:
        case SpvOpDPdx:
        case SpvOpDPdy:
        case SpvOpFwidth:
        case SpvOpPhi:
        case SpvOpLoopMerge:
        case SpvOpSelectionMerge:
        case SpvOpLabel:
        case SpvOpBranch:
        case SpvOpBranchConditional:
        case SpvOpSwitch:
        case SpvOpKill:
        case SpvOpReturn:
        case SpvOpReturnValue:
        case SpvOpUnreachable:
        case SpvOpCopyLogical:
            return true;
        default:
            return false;
        }
    }

    static bool SpvIsIgnored(SpvOp pOpCode) {
        switch (pOpCode) {
        case SpvOpSource:
        case SpvOpName:
        case SpvOpMemberName:
            return true;
        default:
            return false;
        }
    }

    static bool SpvHasOperands(SpvOp pOpCode, uint32_t &rOperandWordStart, uint32_t &rOperandWordCount, uint32_t &rOperandWordStride, uint32_t &rOperandWordSkip, bool &rOperandWordSkipString) {
        switch (pOpCode) {
        case SpvOpExecutionMode:
        case SpvOpDecorate:
        case SpvOpMemberDecorate:
        case SpvOpBranchConditional:
        case SpvOpSwitch:
        case SpvOpReturnValue:
            rOperandWordStart = 1;
            rOperandWordCount = 1;
            rOperandWordStride = 1;
            rOperandWordSkip = UINT32_MAX;
            rOperandWordSkipString = false;
            return true;
        case SpvOpStore:
            rOperandWordStart = 1;
            rOperandWordCount = 2;
            rOperandWordStride = 1;
            rOperandWordSkip = UINT32_MAX;
            rOperandWordSkipString = false;
            return true;
        case SpvOpTypeVector:
        case SpvOpTypeImage:
        case SpvOpTypeSampledImage:
        case SpvOpTypeRuntimeArray:
            rOperandWordStart = 2;
            rOperandWordCount = 1;
            rOperandWordStride = 1;
            rOperandWordSkip = UINT32_MAX;
            rOperandWordSkipString = false;
            return true;
        case SpvOpTypeArray:
            rOperandWordStart = 2;
            rOperandWordCount = 2;
            rOperandWordStride = 1;
            rOperandWordSkip = UINT32_MAX;
            rOperandWordSkipString = false;
            return true;
        case SpvOpTypeStruct:
        case SpvOpTypeFunction:
            rOperandWordStart = 2;
            rOperandWordCount = UINT32_MAX;
            rOperandWordStride = 1;
            rOperandWordSkip = UINT32_MAX;
            rOperandWordSkipString = false;
            return true;
        case SpvOpEntryPoint:
            rOperandWordStart = 2;
            rOperandWordCount = UINT32_MAX;
            rOperandWordStride = 1;
            rOperandWordSkip = 1;
            rOperandWordSkipString = true;
            return true;
        case SpvOpTypePointer:
        case SpvOpLoad:
        case SpvOpCompositeExtract:
        case SpvOpCopyObject:
        case SpvOpTranspose:
        case SpvOpImage:
        case SpvOpImageQueryLevels:
        case SpvOpConvertFToU:
        case SpvOpConvertFToS:
        case SpvOpConvertSToF:
        case SpvOpConvertUToF:
        case SpvOpBitcast:
        case SpvOpSNegate:
        case SpvOpFNegate:
        case SpvOpAny:
        case SpvOpAll:
        case SpvOpLogicalNot:
        case SpvOpNot:
        case SpvOpBitReverse:
        case SpvOpBitCount:
        case SpvOpDPdx:
        case SpvOpDPdy:
        case SpvOpFwidth:
        case SpvOpCopyLogical:
            rOperandWordStart = 3;
            rOperandWordCount = 1;
            rOperandWordStride = 1;
            rOperandWordSkip = UINT32_MAX;
            rOperandWordSkipString = false;
            return true;
        case SpvOpVectorShuffle:
        case SpvOpCompositeInsert:
        case SpvOpSampledImage:
        case SpvOpImageQuerySizeLod:
        case SpvOpIAdd:
        case SpvOpFAdd:
        case SpvOpISub:
        case SpvOpFSub:
        case SpvOpIMul:
        case SpvOpFMul:
        case SpvOpUDiv:
        case SpvOpSDiv:
        case SpvOpFDiv:
        case SpvOpUMod:
        case SpvOpSRem:
        case SpvOpSMod:
        case SpvOpFRem:
        case SpvOpFMod:
        case SpvOpVectorTimesScalar:
        case SpvOpMatrixTimesScalar:
        case SpvOpVectorTimesMatrix:
        case SpvOpMatrixTimesVector:
        case SpvOpMatrixTimesMatrix:
        case SpvOpOuterProduct:
        case SpvOpDot:
        case SpvOpIAddCarry:
        case SpvOpISubBorrow:
        case SpvOpUMulExtended:
        case SpvOpSMulExtended:
        case SpvOpLogicalEqual:
        case SpvOpLogicalNotEqual:
        case SpvOpLogicalOr:
        case SpvOpLogicalAnd:
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
        case SpvOpFOrdEqual:
        case SpvOpFUnordEqual:
        case SpvOpFOrdNotEqual:
        case SpvOpFUnordNotEqual:
        case SpvOpFOrdLessThan:
        case SpvOpFUnordLessThan:
        case SpvOpFOrdGreaterThan:
        case SpvOpFUnordGreaterThan:
        case SpvOpFOrdLessThanEqual:
        case SpvOpFUnordLessThanEqual:
        case SpvOpFOrdGreaterThanEqual:
        case SpvOpFUnordGreaterThanEqual:
        case SpvOpShiftRightLogical:
        case SpvOpShiftRightArithmetic:
        case SpvOpShiftLeftLogical:
        case SpvOpBitwiseOr:
        case SpvOpBitwiseAnd:
        case SpvOpBitwiseXor:
            rOperandWordStart = 3;
            rOperandWordCount = 2;
            rOperandWordStride = 1;
            rOperandWordSkip = UINT32_MAX;
            rOperandWordSkipString = false;
            return true;
        case SpvOpSelect:
        case SpvOpBitFieldSExtract:
        case SpvOpBitFieldUExtract:
            rOperandWordStart = 3;
            rOperandWordCount = 3;
            rOperandWordStride = 1;
            rOperandWordSkip = UINT32_MAX;
            rOperandWordSkipString = false;
            return true;
        case SpvOpBitFieldInsert:
            rOperandWordStart = 3;
            rOperandWordCount = 4;
            rOperandWordStride = 1;
            rOperandWordSkip = UINT32_MAX;
            rOperandWordSkipString = false;
            return true;
        case SpvOpConstantComposite:
        case SpvOpFunctionCall:
        case SpvOpAccessChain:
        case SpvOpCompositeConstruct:
            rOperandWordStart = 3;
            rOperandWordCount = UINT32_MAX;
            rOperandWordStride = 1;
            rOperandWordSkip = UINT32_MAX;
            rOperandWordSkipString = false;
            return true;
        case SpvOpExtInst:
            rOperandWordStart = 3;
            rOperandWordCount = UINT32_MAX;
            rOperandWordStride = 1;
            rOperandWordSkip = 1;
            rOperandWordSkipString = false;
            return true;
        case SpvOpImageWrite:
            rOperandWordStart = 1;
            rOperandWordCount = UINT32_MAX;
            rOperandWordStride = 1;
            rOperandWordSkip = 3;
            rOperandWordSkipString = false;
            return true;
        case SpvOpImageSampleImplicitLod:
        case SpvOpImageSampleExplicitLod:
        case SpvOpImageSampleProjImplicitLod:
        case SpvOpImageSampleProjExplicitLod:
        case SpvOpImageFetch:
        case SpvOpImageRead:
            rOperandWordStart = 3;
            rOperandWordCount = UINT32_MAX;
            rOperandWordStride = 1;
            rOperandWordSkip = 2;
            rOperandWordSkipString = false;
            return true;
        case SpvOpImageSampleDrefImplicitLod:
        case SpvOpImageSampleDrefExplicitLod:
        case SpvOpImageSampleProjDrefImplicitLod:
        case SpvOpImageSampleProjDrefExplicitLod:
        case SpvOpImageGather:
        case SpvOpImageDrefGather:
            rOperandWordStart = 3;
            rOperandWordCount = UINT32_MAX;
            rOperandWordStride = 1;
            rOperandWordSkip = 3;
            rOperandWordSkipString = false;
            return true;
        case SpvOpPhi:
            rOperandWordStart = 3;
            rOperandWordCount = UINT32_MAX;
            rOperandWordStride = 2;
            rOperandWordSkip = UINT32_MAX;
            rOperandWordSkipString = false;
            return true;
        case SpvOpFunction:
        case SpvOpVariable:
            rOperandWordStart = 4;
            rOperandWordCount = 1;
            rOperandWordStride = 1;
            rOperandWordSkip = UINT32_MAX;
            rOperandWordSkipString = false;
            return true;
        default:
            return false;
        }
    }

    static bool SpvHasLabels(SpvOp pOpCode, uint32_t &rLabelWordStart, uint32_t &rLabelWordCount, uint32_t &rLabelWordStride) {
        switch (pOpCode) {
        case SpvOpSelectionMerge:
        case SpvOpBranch:
            rLabelWordStart = 1;
            rLabelWordCount = 1;
            rLabelWordStride = 1;
            return true;
        case SpvOpLoopMerge:
            rLabelWordStart = 1;
            rLabelWordCount = 2;
            rLabelWordStride = 1;
            return true;
        case SpvOpBranchConditional:
            rLabelWordStart = 2;
            rLabelWordCount = 2;
            rLabelWordStride = 1;
            return true;
        case SpvOpSwitch:
            rLabelWordStart = 2;
            rLabelWordCount = UINT32_MAX;
            rLabelWordStride = 2;
            return true;
        default:
            return false;
        }
    }

    static bool SpvOpIsTerminator(SpvOp pOpCode) {
        switch (pOpCode) {
        case SpvOpBranch:
        case SpvOpBranchConditional:
        case SpvOpSwitch:
        case SpvOpReturn:
        case SpvOpReturnValue:
        case SpvOpKill:
        case SpvOpUnreachable:
            return true;
        default:
            return false;
        }
    }

    static bool checkOperandWordSkip(uint32_t pWordIndex, const uint32_t *pSpirvWords, uint32_t pRelativeWordIndex, uint32_t pOperandWordSkip, bool pOperandWordSkipString, uint32_t &rOperandWordIndex) {
        if (pRelativeWordIndex == pOperandWordSkip) {
            if (pOperandWordSkipString) {
                const char *operandString = reinterpret_cast<const char *>(&pSpirvWords[pWordIndex + rOperandWordIndex]);
                uint32_t stringLengthInWords = (strlen(operandString) + sizeof(uint32_t)) / sizeof(uint32_t);
                rOperandWordIndex += stringLengthInWords;
            }
            else {
                rOperandWordIndex++;
            }

            return true;
        }
        else {
            return false;
        }
    }

    // Shader

    Shader::Shader() {
        // Empty.
    }

    Shader::Shader(const void *pData, size_t pSize) {
        parse(pData, pSize);
    }

    void Shader::clear() {
        spirvWords = nullptr;
        spirvWordCount = 0;
        instructions.clear();
        instructionInDegrees.clear();
        instructionOutDegrees.clear();
        instructionOrder.clear();
        results.clear();
        specializations.clear();
        decorations.clear();
        phis.clear();
        listNodes.clear();
    }

    uint32_t Shader::addToList(uint32_t pInstructionIndex, uint32_t pListIndex) {
        listNodes.emplace_back(pInstructionIndex, pListIndex);
        return uint32_t(listNodes.size() - 1);
    }

    bool Shader::parseWords(const void *pData, size_t pSize) {
        assert(pData != nullptr);
        assert(pSize > 0);

        spirvWords = reinterpret_cast<const uint32_t *>(pData);
        spirvWordCount = pSize / sizeof(uint32_t);

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
        while (wordIndex < spirvWordCount) {
            SpvOp opCode = SpvOp(spirvWords[wordIndex] & 0xFFFFU);
            uint32_t wordCount = (spirvWords[wordIndex] >> 16U) & 0xFFFFU;

            bool hasResult, hasType;
            SpvHasResultAndType(opCode, &hasResult, &hasType);

            if (hasResult) {
                uint32_t resultId = spirvWords[wordIndex + (hasType ? 2 : 1)];
                if (resultId >= idBound) {
                    fprintf(stderr, "SPIR-V Parsing error. Invalid Result ID: %u.\n", resultId);
                    return false;
                }

                results[resultId].instructionIndex = uint32_t(instructions.size());
            }

            if ((opCode == SpvOpDecorate) || (opCode == SpvOpMemberDecorate)) {
                decorations.emplace_back(uint32_t(instructions.size()));
            }
            else if (opCode == SpvOpPhi) {
                phis.emplace_back(uint32_t(instructions.size()));
            }

            instructions.emplace_back(wordIndex);
            wordIndex += wordCount;
        }

        return true;
    }

    bool Shader::process() {
        bool foundOpSwitch = false;
        for (uint32_t i = 0; i < uint32_t(instructions.size()); i++) {
            uint32_t wordIndex = instructions[i].wordIndex;
            SpvOp opCode = SpvOp(spirvWords[wordIndex] & 0xFFFFU);
            uint32_t wordCount = (spirvWords[wordIndex] >> 16U) & 0xFFFFU;
            if (!SpvIsSupported(opCode)) {
                fprintf(stderr, "%s is not supported yet.\n", SpvOpToString(opCode));
                return false;
            }

            bool hasResult, hasType;
            SpvHasResultAndType(opCode, &hasResult, &hasType);

            if (hasType) {
                uint32_t typeId = spirvWords[wordIndex + 1];
                if (typeId >= results.size()) {
                    fprintf(stderr, "SPIR-V Parsing error. Invalid Type ID: %u.\n", typeId);
                    return false;
                }

                if (results[typeId].instructionIndex == UINT32_MAX) {
                    fprintf(stderr, "SPIR-V Parsing error. Result %u is not valid.\n", typeId);
                    return false;
                }

                uint32_t typeInstructionIndex = results[typeId].instructionIndex;
                instructions[typeInstructionIndex].adjacentListIndex = addToList(i, instructions[typeInstructionIndex].adjacentListIndex);

                // Check if it's an OpConstant of Int type so it can be reused on switches.
                if ((opCode == SpvOpConstant) && (defaultSwitchOpConstantInt == UINT32_MAX)) {
                    uint32_t typeWordIndex = instructions[typeInstructionIndex].wordIndex;
                    SpvOp typeOpCode = SpvOp(spirvWords[typeWordIndex] & 0xFFFFU);
                    if (typeOpCode == SpvOpTypeInt) {
                        defaultSwitchOpConstantInt = spirvWords[wordIndex + 2];
                    }
                }
            }
            
            // Every operand should be adjacent to this instruction.
            uint32_t operandWordStart, operandWordCount, operandWordStride, operandWordSkip;
            bool operandWordSkipString;
            if (SpvHasOperands(opCode, operandWordStart, operandWordCount, operandWordStride, operandWordSkip, operandWordSkipString)) {
                uint32_t operandWordIndex = operandWordStart;
                for (uint32_t j = 0; j < operandWordCount; j++) {
                    if (checkOperandWordSkip(wordIndex, spirvWords, j, operandWordSkip, operandWordSkipString, operandWordIndex)) {
                        continue;
                    }

                    if (operandWordIndex >= wordCount) {
                        break;
                    }

                    uint32_t operandId = spirvWords[wordIndex + operandWordIndex];
                    if (operandId >= results.size()) {
                        fprintf(stderr, "SPIR-V Parsing error. Invalid Operand ID: %u.\n", operandId);
                        return false;
                    }

                    if (results[operandId].instructionIndex == UINT32_MAX) {
                        fprintf(stderr, "SPIR-V Parsing error. Result %u is not valid.\n", operandId);
                        return false;
                    }

                    uint32_t resultIndex = results[operandId].instructionIndex;
                    instructions[resultIndex].adjacentListIndex = addToList(i, instructions[resultIndex].adjacentListIndex);
                    operandWordIndex += operandWordStride;
                }
            }

            // This instruction should be adjacent to every label referenced. OpPhi is excluded from this.
            uint32_t labelWordStart, labelWordCount, labelWordStride;
            if (SpvHasLabels(opCode, labelWordStart, labelWordCount, labelWordStride)) {
                for (uint32_t j = 0; (j < labelWordCount) && ((labelWordStart + j * labelWordStride) < wordCount); j++) {
                    uint32_t labelId = spirvWords[wordIndex + labelWordStart + j * labelWordStride];
                    if (labelId >= results.size()) {
                        fprintf(stderr, "SPIR-V Parsing error. Invalid Operand ID: %u.\n", labelId);
                        return false;
                    }

                    if (results[labelId].instructionIndex == UINT32_MAX) {
                        fprintf(stderr, "SPIR-V Parsing error. Invalid Operand ID: %u.\n", labelId);
                        return false;
                    }

                    uint32_t labelIndex = results[labelId].instructionIndex;
                    instructions[i].adjacentListIndex = addToList(labelIndex, instructions[i].adjacentListIndex);
                }
            }

            // Parse parented blocks of OpPhi to indicate the dependency.
            if (opCode == SpvOpPhi) {
                for (uint32_t j = 3; j < wordCount; j += 2) {
                    uint32_t labelId = spirvWords[wordIndex + j + 1];
                    if (labelId >= results.size()) {
                        fprintf(stderr, "SPIR-V Parsing error. Invalid Parent ID: %u.\n", labelId);
                        return false;
                    }

                    if (results[labelId].instructionIndex == UINT32_MAX) {
                        fprintf(stderr, "SPIR-V Parsing error. Invalid Parent ID: %u.\n", labelId);
                        return false;
                    }

                    uint32_t labelIndex = results[labelId].instructionIndex;
                    instructions[labelIndex].adjacentListIndex = addToList(i, instructions[labelIndex].adjacentListIndex);
                }
            }
            // Parse decorations.
            else if (opCode == SpvOpDecorate) {
                uint32_t decoration = spirvWords[wordIndex + 2];
                if (decoration == SpvDecorationSpecId) {
                    uint32_t resultId = spirvWords[wordIndex + 1];
                    uint32_t constantId = spirvWords[wordIndex + 3];
                    if (resultId >= results.size()) {
                        fprintf(stderr, "SPIR-V Parsing error. Invalid Operand ID: %u.\n", resultId);
                        return false;
                    }

                    uint32_t resultInstructionIndex = results[resultId].instructionIndex;
                    if (resultInstructionIndex == UINT32_MAX) {
                        fprintf(stderr, "SPIR-V Parsing error. Invalid Operand ID: %u.\n", resultId);
                        return false;
                    }

                    specializations.resize(std::max(specializations.size(), size_t(constantId + 1)));
                    specializations[constantId].constantInstructionIndex = resultInstructionIndex;
                    specializations[constantId].decorationInstructionIndex = i;
                }
            }
            // Check if a switch is used in the shader.
            else if (opCode == SpvOpSwitch) {
                foundOpSwitch = true;
            }
        }

        if (foundOpSwitch && (defaultSwitchOpConstantInt == UINT32_MAX)) {
            fprintf(stderr, "Unable to find an OpConstantInt to use as replacement for switches. Adding this instruction automatically is not supported yet.\n");
            return false;
        }

        return true;
    }

    struct InstructionSort {
        uint32_t instructionIndex = 0;
        uint32_t instructionLevel = 0;

        InstructionSort() {
            // Empty.
        }

        InstructionSort(uint32_t instructionIndex, uint32_t instructionLevel) {
            this->instructionIndex = instructionIndex;
            this->instructionLevel = instructionLevel;
        }

        bool operator<(const InstructionSort &i) const {
            if (instructionLevel < i.instructionLevel) {
                return true;
            }
            else if (instructionLevel > i.instructionLevel) {
                return false;
            }

            return instructionIndex < i.instructionIndex;
        }
    };

    bool Shader::sort() {
        // Count the in and out degrees for all instructions.
        instructionInDegrees.clear();
        instructionOutDegrees.clear();
        instructionInDegrees.resize(instructions.size(), 0);
        instructionOutDegrees.resize(instructions.size(), 0);
        for (uint32_t i = 0; i < uint32_t(instructions.size()); i++) {
            uint32_t listIndex = instructions[i].adjacentListIndex;
            while (listIndex != UINT32_MAX) {
                const ListNode &listNode = listNodes[listIndex];
                instructionInDegrees[listNode.instructionIndex]++;
                instructionOutDegrees[i]++;
                listIndex = listNode.nextListIndex;
            }
        }

        // Make a copy of the degrees as they'll be used to perform a topological sort.
        std::vector<uint32_t> sortDegrees;
        sortDegrees.resize(instructionInDegrees.size());
        memcpy(sortDegrees.data(), instructionInDegrees.data(), sizeof(uint32_t) *sortDegrees.size());

        // The first nodes to be processed should be the ones with no incoming connections.
        std::vector<uint32_t> instructionStack;
        instructionStack.clear();
        for (uint32_t i = 0; i < uint32_t(instructions.size()); i++) {
            if (sortDegrees[i] == 0) {
                instructionStack.emplace_back(i);
            }
        }

        instructionOrder.reserve(instructions.size());
        instructionOrder.clear();
        while (!instructionStack.empty()) {
            uint32_t i = instructionStack.back();
            instructionStack.pop_back();
            instructionOrder.emplace_back(i);

            // Look for the adjacents and reduce their degree. Push it to the stack if their degree reaches zero.
            uint32_t listIndex = instructions[i].adjacentListIndex;
            while (listIndex != UINT32_MAX) {
                const ListNode &listNode = listNodes[listIndex];
                uint32_t &sortDegree = sortDegrees[listNode.instructionIndex];
                assert(sortDegree > 0);
                sortDegree--;
                if (sortDegree == 0) {
                    instructionStack.emplace_back(listNode.instructionIndex);
                }

                listIndex = listNode.nextListIndex;
            }
        }

        std::vector<InstructionSort> instructionSortVector;
        instructionSortVector.clear();
        instructionSortVector.resize(instructionOrder.size(), InstructionSort());
        for (uint32_t instructionIndex : instructionOrder) {
            uint32_t nextLevel = instructionSortVector[instructionIndex].instructionLevel + 1;
            uint32_t listIndex = instructions[instructionIndex].adjacentListIndex;
            while (listIndex != UINT32_MAX) {
                const ListNode &listNode = listNodes[listIndex];
                uint32_t &listLevel = instructionSortVector[listNode.instructionIndex].instructionLevel;
                listLevel = std::max(listLevel, nextLevel);
                listIndex = listNode.nextListIndex;
            }

            instructionSortVector[instructionIndex].instructionIndex = instructionIndex;
        }

        std::sort(instructionSortVector.begin(), instructionSortVector.end());
        
        // Rebuild the instruction order vector with the sorted indices.
        instructionOrder.clear();
        for (InstructionSort &instructionSort : instructionSortVector) {
            instructionOrder.emplace_back(instructionSort.instructionIndex);
        }

        return true;
    }

    bool Shader::parse(const void *pData, size_t pSize) {
        assert(pData != nullptr);
        assert((pSize % sizeof(uint32_t) == 0) && "Size of data must be aligned to the word size.");

        clear();

        if (!parseWords(pData, pSize)) {
            return false;
        }

        if (!process()) {
            return false;
        }

        if (!sort()) {
            return false;
        }

        return true;
    }

    bool Shader::empty() const {
        return false;
    }

    // Optimizer

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

        static Resolution fromBool(bool pValue) {
            Resolution r;
            r.type = Type::Constant;
            r.value.u32 = pValue ? 1 : 0;
            return r;
        }

        static Resolution fromInt32(int32_t pValue) {
            Resolution r;
            r.type = Type::Constant;
            r.value.i32 = pValue;
            return r;
        }

        static Resolution fromUint32(uint32_t pValue) {
            Resolution r;
            r.type = Type::Constant;
            r.value.u32 = pValue;
            return r;
        }
    };

    struct OptimizerContext {
        const Shader &shader;
        std::vector<uint32_t> &instructionInDegrees;
        std::vector<uint32_t> &instructionOutDegrees;
        std::vector<Resolution> &resolutions;
        std::vector<uint8_t> &optimizedData;
        Options options;

        OptimizerContext() = delete;
    };

    static void optimizerEliminateInstruction(uint32_t pInstructionIndex, OptimizerContext &rContext) {
        uint32_t *optimizedWords = reinterpret_cast<uint32_t *>(rContext.optimizedData.data());
        uint32_t wordIndex = rContext.shader.instructions[pInstructionIndex].wordIndex;
        uint32_t wordCount = (optimizedWords[wordIndex] >> 16U) & 0xFFFFU;
        for (uint32_t j = 0; j < wordCount; j++) {
            optimizedWords[wordIndex + j] = UINT32_MAX;
        }
    }

    static void optimizerReduceResultDegrees(OptimizerContext &rContext, std::vector<uint32_t> &rResultStack) {
        const uint32_t *optimizedWords = reinterpret_cast<const uint32_t *>(rContext.optimizedData.data());
        while (!rResultStack.empty()) {
            uint32_t resultId = rResultStack.back();
            rResultStack.pop_back();

            uint32_t instructionIndex = rContext.shader.results[resultId].instructionIndex;
            uint32_t wordIndex = rContext.shader.instructions[instructionIndex].wordIndex;

            // Instruction's been deleted.
            if (optimizedWords[wordIndex] == UINT32_MAX) {
                continue;
            }

            rContext.instructionOutDegrees[instructionIndex]--;

            // When nothing uses the result from this instruction anymore, we can delete it. Push any operands it uses into the stack as well to reduce their out degrees.
            if (rContext.instructionOutDegrees[instructionIndex] == 0) {
                SpvOp opCode = SpvOp(optimizedWords[wordIndex] & 0xFFFFU);
                uint32_t wordCount = (optimizedWords[wordIndex] >> 16U) & 0xFFFFU;
                uint32_t operandWordStart, operandWordCount, operandWordStride, operandWordSkip;
                bool operandWordSkipString;
                if (SpvHasOperands(opCode, operandWordStart, operandWordCount, operandWordStride, operandWordSkip, operandWordSkipString)) {
                    uint32_t operandWordIndex = operandWordStart;
                    for (uint32_t j = 0; j < operandWordCount; j++) {
                        if (checkOperandWordSkip(wordIndex, optimizedWords, j, operandWordSkip, operandWordSkipString, operandWordIndex)) {
                            continue;
                        }

                        if (operandWordIndex >= wordCount) {
                            break;
                        }

                        uint32_t operandId = optimizedWords[wordIndex + operandWordIndex];
                        rResultStack.emplace_back(operandId);
                        operandWordIndex += operandWordStride;
                    }
                }

                optimizerEliminateInstruction(instructionIndex, rContext);
            }
        }
    }

    static bool optimizerPrepareData(OptimizerContext &rContext) {
        OptimizerContext &c = rContext;
        c.resolutions.clear();
        c.resolutions.resize(c.shader.results.size(), Resolution());
        c.instructionInDegrees.resize(c.shader.instructionInDegrees.size());
        c.instructionOutDegrees.resize(c.shader.instructionOutDegrees.size());
        c.optimizedData.resize(c.shader.spirvWordCount * sizeof(uint32_t));
        memcpy(c.instructionInDegrees.data(), c.shader.instructionInDegrees.data(), sizeof(uint32_t) * c.shader.instructionInDegrees.size());
        memcpy(c.instructionOutDegrees.data(), c.shader.instructionOutDegrees.data(), sizeof(uint32_t) * c.shader.instructionOutDegrees.size());
        memcpy(c.optimizedData.data(), c.shader.spirvWords, c.optimizedData.size());
        return true;
    }

    static bool optimizerPatchSpecializationConstants(const SpecConstant *pNewSpecConstants, uint32_t pNewSpecConstantCount, OptimizerContext &rContext) {
        uint32_t *optimizedWords = reinterpret_cast<uint32_t *>(rContext.optimizedData.data());
        for (uint32_t i = 0; i < pNewSpecConstantCount; i++) {
            const SpecConstant &newSpecConstant = pNewSpecConstants[i];
            if (newSpecConstant.specId >= rContext.shader.specializations.size()) {
                continue;
            }

            const Specialization &specialization = rContext.shader.specializations[newSpecConstant.specId];
            if (specialization.constantInstructionIndex == UINT32_MAX) {
                continue;
            }

            uint32_t constantWordIndex = rContext.shader.instructions[specialization.constantInstructionIndex].wordIndex;
            SpvOp constantOpCode = SpvOp(optimizedWords[constantWordIndex] & 0xFFFFU);
            uint32_t constantWordCount = (optimizedWords[constantWordIndex] >> 16U) & 0xFFFFU;
            switch (constantOpCode) {
            case SpvOpSpecConstantTrue:
            case SpvOpSpecConstantFalse:
                optimizedWords[constantWordIndex] = (newSpecConstant.values[0] ? SpvOpConstantTrue : SpvOpConstantFalse) | (constantWordCount << 16U);
                break;
            case SpvOpSpecConstant:
                if (constantWordCount <= 3) {
                    fprintf(stderr, "Optimization error. Specialization constant has less words than expected.\n");
                    return false;
                }

                if (newSpecConstant.values.size() != (constantWordCount - 3)) {
                    fprintf(stderr, "Optimization error. Value count for specialization constant %u differs from the expected size.\n", newSpecConstant.specId);
                    return false;
                }

                optimizedWords[constantWordIndex] = SpvOpConstant | (constantWordCount << 16U);
                memcpy(&optimizedWords[constantWordIndex + 3], newSpecConstant.values.data(), sizeof(uint32_t) * (constantWordCount - 3));
                break;
            default:
                fprintf(stderr, "Optimization error. Can't patch opCode %u.\n", constantOpCode);
                return false;
            }

            // Eliminate the decorator instruction as well.
            optimizerEliminateInstruction(specialization.decorationInstructionIndex, rContext);
        }

        return true;
    }

    static void optimizerEvaluateResult(uint32_t pResultId, OptimizerContext &rContext) {
        const uint32_t *optimizedWords = reinterpret_cast<const uint32_t *>(rContext.optimizedData.data());
        const Result &result = rContext.shader.results[pResultId];
        Resolution &resolution = rContext.resolutions[pResultId];
        uint32_t resultWordIndex = rContext.shader.instructions[result.instructionIndex].wordIndex;
        SpvOp opCode = SpvOp(optimizedWords[resultWordIndex] & 0xFFFFU);
        uint32_t wordCount = (optimizedWords[resultWordIndex] >> 16U) & 0xFFFFU;
        switch (opCode) {
        case SpvOpConstant: {
            // Parse the known type of constants. Any other types will be considered as variable.
            const Result &typeResult = rContext.shader.results[optimizedWords[resultWordIndex + 1]];
            uint32_t typeWordIndex = rContext.shader.instructions[typeResult.instructionIndex].wordIndex;
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
        case SpvOpBitcast: {
            const Resolution &operandResolution = rContext.resolutions[optimizedWords[resultWordIndex + 3]];
            resolution = Resolution::fromUint32(operandResolution.value.u32);
            break;
        }
        case SpvOpIAdd: {
            const Resolution &firstResolution = rContext.resolutions[optimizedWords[resultWordIndex + 3]];
            const Resolution &secondResolution = rContext.resolutions[optimizedWords[resultWordIndex + 4]];
            resolution = Resolution::fromUint32(firstResolution.value.u32 + secondResolution.value.u32);
            break;
        }
        case SpvOpISub: {
            const Resolution &firstResolution = rContext.resolutions[optimizedWords[resultWordIndex + 3]];
            const Resolution &secondResolution = rContext.resolutions[optimizedWords[resultWordIndex + 4]];
            resolution = Resolution::fromUint32(firstResolution.value.u32 - secondResolution.value.u32);
            break;
        }
        case SpvOpIMul: {
            const Resolution &firstResolution = rContext.resolutions[optimizedWords[resultWordIndex + 3]];
            const Resolution &secondResolution = rContext.resolutions[optimizedWords[resultWordIndex + 4]];
            resolution = Resolution::fromUint32(firstResolution.value.u32 * secondResolution.value.u32);
            break;
        }
        case SpvOpUDiv: {
            const Resolution &firstResolution = rContext.resolutions[optimizedWords[resultWordIndex + 3]];
            const Resolution &secondResolution = rContext.resolutions[optimizedWords[resultWordIndex + 4]];
            resolution = Resolution::fromUint32(firstResolution.value.u32 / secondResolution.value.u32);
            break;
        }
        case SpvOpSDiv: {
            const Resolution &firstResolution = rContext.resolutions[optimizedWords[resultWordIndex + 3]];
            const Resolution &secondResolution = rContext.resolutions[optimizedWords[resultWordIndex + 4]];
            resolution = Resolution::fromUint32(firstResolution.value.i32 / secondResolution.value.i32);
            break;
        }
        case SpvOpLogicalEqual: {
            const Resolution &firstResolution = rContext.resolutions[optimizedWords[resultWordIndex + 3]];
            const Resolution &secondResolution = rContext.resolutions[optimizedWords[resultWordIndex + 4]];
            resolution = Resolution::fromBool((firstResolution.value.u32 != 0) == (secondResolution.value.u32 != 0));
            break;
        }
        case SpvOpLogicalNotEqual: {
            const Resolution &firstResolution = rContext.resolutions[optimizedWords[resultWordIndex + 3]];
            const Resolution &secondResolution = rContext.resolutions[optimizedWords[resultWordIndex + 4]];
            resolution = Resolution::fromBool((firstResolution.value.u32 != 0) != (secondResolution.value.u32 != 0));
            break;
        }
        case SpvOpLogicalOr: {
            const Resolution &firstResolution = rContext.resolutions[optimizedWords[resultWordIndex + 3]];
            const Resolution &secondResolution = rContext.resolutions[optimizedWords[resultWordIndex + 4]];
            resolution = Resolution::fromBool((firstResolution.value.u32 != 0) || (secondResolution.value.u32 != 0));
            break;
        }
        case SpvOpLogicalAnd: {
            const Resolution &firstResolution = rContext.resolutions[optimizedWords[resultWordIndex + 3]];
            const Resolution &secondResolution = rContext.resolutions[optimizedWords[resultWordIndex + 4]];
            resolution = Resolution::fromBool((firstResolution.value.u32 != 0) && (secondResolution.value.u32 != 0));
            break;
        }
        case SpvOpLogicalNot: {
            const Resolution &operandResolution = rContext.resolutions[optimizedWords[resultWordIndex + 3]];
            resolution = Resolution::fromBool(operandResolution.value.u32 == 0);
            break;
        }
        case SpvOpSelect: {
            const Resolution &conditionResolution = rContext.resolutions[optimizedWords[resultWordIndex + 3]];
            const Resolution &firstResolution = rContext.resolutions[optimizedWords[resultWordIndex + 4]];
            const Resolution &secondResolution = rContext.resolutions[optimizedWords[resultWordIndex + 5]];
            resolution = (conditionResolution.value.u32 != 0) ? firstResolution : secondResolution;
            break;
        }
        case SpvOpIEqual: {
            const Resolution &firstResolution = rContext.resolutions[optimizedWords[resultWordIndex + 3]];
            const Resolution &secondResolution = rContext.resolutions[optimizedWords[resultWordIndex + 4]];
            resolution = Resolution::fromBool(firstResolution.value.u32 == secondResolution.value.u32);
            break;
        }
        case SpvOpINotEqual: {
            const Resolution &firstResolution = rContext.resolutions[optimizedWords[resultWordIndex + 3]];
            const Resolution &secondResolution = rContext.resolutions[optimizedWords[resultWordIndex + 4]];
            resolution = Resolution::fromBool(firstResolution.value.u32 != secondResolution.value.u32);
            break;
        }
        case SpvOpUGreaterThan: {
            const Resolution &firstResolution = rContext.resolutions[optimizedWords[resultWordIndex + 3]];
            const Resolution &secondResolution = rContext.resolutions[optimizedWords[resultWordIndex + 4]];
            resolution = Resolution::fromBool(firstResolution.value.u32 > secondResolution.value.u32);
            break;
        }
        case SpvOpSGreaterThan: {
            const Resolution &firstResolution = rContext.resolutions[optimizedWords[resultWordIndex + 3]];
            const Resolution &secondResolution = rContext.resolutions[optimizedWords[resultWordIndex + 4]];
            resolution = Resolution::fromBool(firstResolution.value.i32 > secondResolution.value.i32);
            break;
        }
        case SpvOpUGreaterThanEqual: {
            const Resolution &firstResolution = rContext.resolutions[optimizedWords[resultWordIndex + 3]];
            const Resolution &secondResolution = rContext.resolutions[optimizedWords[resultWordIndex + 4]];
            resolution = Resolution::fromBool(firstResolution.value.u32 >= secondResolution.value.u32);
            break;
        }
        case SpvOpSGreaterThanEqual: {
            const Resolution &firstResolution = rContext.resolutions[optimizedWords[resultWordIndex + 3]];
            const Resolution &secondResolution = rContext.resolutions[optimizedWords[resultWordIndex + 4]];
            resolution = Resolution::fromBool(firstResolution.value.i32 >= secondResolution.value.i32);
            break;
        }
        case SpvOpULessThan: {
            const Resolution &firstResolution = rContext.resolutions[optimizedWords[resultWordIndex + 3]];
            const Resolution &secondResolution = rContext.resolutions[optimizedWords[resultWordIndex + 4]];
            resolution = Resolution::fromBool(firstResolution.value.u32 < secondResolution.value.u32);
            break;
        }
        case SpvOpSLessThan: {
            const Resolution &firstResolution = rContext.resolutions[optimizedWords[resultWordIndex + 3]];
            const Resolution &secondResolution = rContext.resolutions[optimizedWords[resultWordIndex + 4]];
            resolution = Resolution::fromBool(firstResolution.value.i32 < secondResolution.value.i32);
            break;
        }
        case SpvOpULessThanEqual: {
            const Resolution &firstResolution = rContext.resolutions[optimizedWords[resultWordIndex + 3]];
            const Resolution &secondResolution = rContext.resolutions[optimizedWords[resultWordIndex + 4]];
            resolution = Resolution::fromBool(firstResolution.value.u32 <= secondResolution.value.u32);
            break;
        }
        case SpvOpSLessThanEqual: {
            const Resolution &firstResolution = rContext.resolutions[optimizedWords[resultWordIndex + 3]];
            const Resolution &secondResolution = rContext.resolutions[optimizedWords[resultWordIndex + 4]];
            resolution = Resolution::fromBool(firstResolution.value.i32 <= secondResolution.value.i32);
            break;
        }
        case SpvOpShiftRightLogical: {
            const Resolution &baseResolution = rContext.resolutions[optimizedWords[resultWordIndex + 3]];
            const Resolution &shiftResolution = rContext.resolutions[optimizedWords[resultWordIndex + 4]];
            resolution = Resolution::fromUint32(baseResolution.value.u32 >> shiftResolution.value.u32);
            break;
        }
        case SpvOpShiftRightArithmetic: {
            const Resolution &baseResolution = rContext.resolutions[optimizedWords[resultWordIndex + 3]];
            const Resolution &shiftResolution = rContext.resolutions[optimizedWords[resultWordIndex + 4]];
            resolution = Resolution::fromInt32(baseResolution.value.i32 >> shiftResolution.value.i32);
            break;
        }
        case SpvOpShiftLeftLogical: {
            const Resolution &baseResolution = rContext.resolutions[optimizedWords[resultWordIndex + 3]];
            const Resolution &shiftResolution = rContext.resolutions[optimizedWords[resultWordIndex + 4]];
            resolution = Resolution::fromUint32(baseResolution.value.u32 << shiftResolution.value.u32);
            break;
        }
        case SpvOpBitwiseOr: {
            const Resolution &firstResolution = rContext.resolutions[optimizedWords[resultWordIndex + 3]];
            const Resolution &secondResolution = rContext.resolutions[optimizedWords[resultWordIndex + 4]];
            resolution = Resolution::fromUint32(firstResolution.value.u32 | secondResolution.value.u32);
            break;
        }
        case SpvOpBitwiseAnd: {
            const Resolution &firstResolution = rContext.resolutions[optimizedWords[resultWordIndex + 3]];
            const Resolution &secondResolution = rContext.resolutions[optimizedWords[resultWordIndex + 4]];
            resolution = Resolution::fromUint32(firstResolution.value.u32 & secondResolution.value.u32);
            break;
        }
        case SpvOpBitwiseXor: {
            const Resolution &firstResolution = rContext.resolutions[optimizedWords[resultWordIndex + 3]];
            const Resolution &secondResolution = rContext.resolutions[optimizedWords[resultWordIndex + 4]];
            resolution = Resolution::fromUint32(firstResolution.value.u32 ^ secondResolution.value.u32);
            break;
        }
        case SpvOpNot: {
            const Resolution &operandResolution = rContext.resolutions[optimizedWords[resultWordIndex + 3]];
            resolution = Resolution::fromUint32(~operandResolution.value.u32);
            break;
        }
        case SpvOpPhi: {
            // Resolve as constant if Phi operator was compacted to only one option.
            if (wordCount == 5) {
                resolution = rContext.resolutions[optimizedWords[resultWordIndex + 3]];
            }
            else {
                resolution.type = Resolution::Type::Variable;
            }

            break;
        }
        default:
            // It's not known how to evaluate the instruction, consider the result a variable.
            resolution.type = Resolution::Type::Variable;
            break;
        }
    }

    static void optimizerReduceLabelDegree(uint32_t pFirstLabelId, OptimizerContext &rContext) {
        thread_local std::vector<uint32_t> labelStack;
        thread_local std::vector<uint32_t> resultStack;
        thread_local std::vector<uint32_t> degreeReductions;
        labelStack.emplace_back(pFirstLabelId);
        resultStack.clear();
        degreeReductions.clear();

        uint32_t *optimizedWords = reinterpret_cast<uint32_t *>(rContext.optimizedData.data());
        while (!labelStack.empty()) {
            uint32_t labelId = labelStack.back();
            labelStack.pop_back();

            uint32_t instructionIndex = rContext.shader.results[labelId].instructionIndex;
            if (rContext.instructionInDegrees[instructionIndex] == 0) {
                continue;
            }

            rContext.instructionInDegrees[instructionIndex]--;

            // If a label's degree becomes 0, eliminate all the instructions of the block.
            // Eliminate as many instructions as possible until finding the terminator of the block.
            // When finding the terminator, look at the labels it has and push them to the stack to
            // reduce their degrees as well.
            if (rContext.instructionInDegrees[instructionIndex] == 0) {
                bool foundTerminator = false;
                uint32_t instructionCount = rContext.shader.instructions.size();
                for (uint32_t i = instructionIndex; (i < instructionCount) && !foundTerminator; i++) {
                    uint32_t wordIndex = rContext.shader.instructions[i].wordIndex;
                    if (optimizedWords[wordIndex] == UINT32_MAX) {
                        continue;
                    }

                    // If the instruction has labels it can reference, we push the labels to reduce their degrees as well.
                    SpvOp opCode = SpvOp(optimizedWords[wordIndex] & 0xFFFFU);
                    uint32_t wordCount = (optimizedWords[wordIndex] >> 16U) & 0xFFFFU;
                    uint32_t labelWordStart, labelWordCount, labelWordStride;
                    if (SpvHasLabels(opCode, labelWordStart, labelWordCount, labelWordStride)) {
                        for (uint32_t j = 0; (j < labelWordCount) && ((labelWordStart + j * labelWordStride) < wordCount); j++) {
                            uint32_t terminatorLabelId = optimizedWords[wordIndex + labelWordStart + j * labelWordStride];
                            labelStack.emplace_back(terminatorLabelId);
                        }
                    }

                    // If the instruction has operands, decrease their degree.
                    uint32_t operandWordStart, operandWordCount, operandWordStride, operandWordSkip;
                    bool operandWordSkipString;
                    if (SpvHasOperands(opCode, operandWordStart, operandWordCount, operandWordStride, operandWordSkip, operandWordSkipString)) {
                        uint32_t operandWordIndex = operandWordStart;
                        for (uint32_t j = 0; j < operandWordCount; j++) {
                            if (checkOperandWordSkip(wordIndex, optimizedWords, j, operandWordSkip, operandWordSkipString, operandWordIndex)) {
                                continue;
                            }

                            if (operandWordIndex >= wordCount) {
                                break;
                            }

                            uint32_t operandId = optimizedWords[wordIndex + operandWordIndex];
                            resultStack.emplace_back(operandId);
                            operandWordIndex += operandWordStride;
                        }
                    }

                    foundTerminator = SpvOpIsTerminator(opCode);
                    optimizerEliminateInstruction(i, rContext);
                }
            }
        }

        optimizerReduceResultDegrees(rContext, resultStack);
    }

    static void optimizerEvaluateTerminator(uint32_t pInstructionIndex, OptimizerContext &rContext) {
        // For each type of supported terminator, check if the operands can be resolved into constants.
        // If they can be resolved, eliminate any other branches that don't pass the condition.
        uint32_t wordIndex = rContext.shader.instructions[pInstructionIndex].wordIndex;
        uint32_t *optimizedWords = reinterpret_cast<uint32_t *>(rContext.optimizedData.data());
        SpvOp opCode = SpvOp(optimizedWords[wordIndex] & 0xFFFFU);
        uint32_t wordCount = (optimizedWords[wordIndex] >> 16U) & 0xFFFFU;
        uint32_t defaultLabelId = UINT32_MAX;

        // Both instructions share that the second word is the operator they must use to resolve the condition.
        // Operator can't be anything but a constant to be able to resolve a terminator.
        const uint32_t operatorId = optimizedWords[wordIndex + 1];
        const Resolution &operatorResolution = rContext.resolutions[operatorId];
        if (operatorResolution.type != Resolution::Type::Constant) {
            return;
        }
        
        if (opCode == SpvOpBranchConditional) {
            // Branch conditional only needs to choose either label depending on whether the result is true or false.
            if (operatorResolution.value.u32) {
                defaultLabelId = optimizedWords[wordIndex + 2];
                optimizerReduceLabelDegree(optimizedWords[wordIndex + 3], rContext);
            }
            else {
                defaultLabelId = optimizedWords[wordIndex + 3];
                optimizerReduceLabelDegree(optimizedWords[wordIndex + 2], rContext);
            }

            // If there's a selection merge before this branch, we place the unconditional branch in its place.
            const uint32_t mergeWordCount = 3;
            uint32_t mergeWordIndex = wordIndex - mergeWordCount;
            SpvOp mergeOpCode = SpvOp(optimizedWords[mergeWordIndex] & 0xFFFFU);

            uint32_t patchWordIndex;
            if (mergeOpCode == SpvOpSelectionMerge) {
                optimizerReduceLabelDegree(optimizedWords[mergeWordIndex + 1], rContext);
                patchWordIndex = mergeWordIndex;
            }
            else {
                patchWordIndex = wordIndex;
            }

            // Make the final label the new default case and reduce the word count.
            optimizedWords[patchWordIndex] = SpvOpBranch | (2U << 16U);
            optimizedWords[patchWordIndex + 1] = defaultLabelId;

            // Eliminate any remaining words on the block.
            for (uint32_t i = patchWordIndex + 2; i < (wordIndex + wordCount); i++) {
                optimizedWords[i] = UINT32_MAX;
            }
        }
        else if (opCode == SpvOpSwitch) {
            // Switch must compare the integer result of the operator to all the possible labels.
            // If the label is not as possible result, then reduce its block's degree.
            for (uint32_t i = 3; i < wordCount; i += 2) {
                if (operatorResolution.value.u32 == optimizedWords[wordIndex + i]) {
                    defaultLabelId = optimizedWords[wordIndex + i + 1];
                }
                else {
                    optimizerReduceLabelDegree(optimizedWords[wordIndex + i + 1], rContext);
                }
            }

            // If none are chosen, the default label is selected. Otherwise, reduce the block's degree
            // for the default label.
            if (defaultLabelId == UINT32_MAX) {
                defaultLabelId = optimizedWords[wordIndex + 2];
            }
            else {
                optimizerReduceLabelDegree(optimizedWords[wordIndex + 2], rContext);
            }

            // Make the final label the new default case and reduce the word count.
            optimizedWords[wordIndex] = SpvOpSwitch | (3U << 16U);
            optimizedWords[wordIndex + 1] = rContext.shader.defaultSwitchOpConstantInt;
            optimizedWords[wordIndex + 2] = defaultLabelId;

            // Increase the degree of the default constant that was chosen so it's not considered as dead code.
            uint32_t defaultConstantInstructionIndex = rContext.shader.results[rContext.shader.defaultSwitchOpConstantInt].instructionIndex;
            rContext.instructionOutDegrees[defaultConstantInstructionIndex]++;

            // Eliminate any remaining words on the block.
            for (uint32_t i = wordIndex + 3; i < (wordIndex + wordCount); i++) {
                optimizedWords[i] = UINT32_MAX;
            }
        }

        // The condition operator can be discarded.
        thread_local std::vector<uint32_t> resultStack;
        resultStack.clear();
        resultStack.emplace_back(operatorId);
        optimizerReduceResultDegrees(rContext, resultStack);
    }

    static bool optimizerCompactPhi(uint32_t pInstructionIndex, OptimizerContext &rContext) {
        // Do a backwards search first to find out what label this instruction belongs to.
        uint32_t *optimizedWords = reinterpret_cast<uint32_t *>(rContext.optimizedData.data());
        uint32_t searchInstructionIndex = pInstructionIndex;
        uint32_t instructionLabelId = UINT32_MAX;
        while (searchInstructionIndex > 0) {
            uint32_t searchWordIndex = rContext.shader.instructions[searchInstructionIndex].wordIndex;
            SpvOp searchOpCode = SpvOp(optimizedWords[searchWordIndex] & 0xFFFFU);
            if (searchOpCode == SpvOpLabel) {
                instructionLabelId = optimizedWords[searchWordIndex + 1];
                break;
            }

            searchInstructionIndex--;
        }

        if (instructionLabelId == UINT32_MAX) {
            fprintf(stderr, "Unable to find a label before OpPhi.\n");
            return false;
        }

        thread_local std::vector<uint32_t> resultStack;
        resultStack.clear();

        uint32_t wordIndex = rContext.shader.instructions[pInstructionIndex].wordIndex;
        uint32_t wordCount = (optimizedWords[wordIndex] >> 16U) & 0xFFFFU;
        uint32_t newWordCount = 3;
        uint32_t instructionCount = rContext.shader.instructions.size();
        for (uint32_t i = 3; i < wordCount; i += 2) {
            uint32_t labelId = optimizedWords[wordIndex + i + 1];
            uint32_t labelInstructionIndex = rContext.shader.results[labelId].instructionIndex;
            uint32_t labelWordIndex = rContext.shader.instructions[labelInstructionIndex].wordIndex;

            // Label's been eliminated. Skip it.
            if (optimizedWords[labelWordIndex] == UINT32_MAX) {
                resultStack.emplace_back(optimizedWords[wordIndex + i]);
                continue;
            }

            // While the label may not have been eliminated, verify its terminator is still pointing to this block.
            bool foundBranchToThisBlock = false;
            for (uint32_t j = labelInstructionIndex; j < instructionCount; j++) {
                uint32_t searchWordIndex = rContext.shader.instructions[j].wordIndex;
                SpvOp searchOpCode = SpvOp(optimizedWords[searchWordIndex] & 0xFFFFU);
                uint32_t searchWordCount = (optimizedWords[searchWordIndex] >> 16U) & 0xFFFFU;
                if (SpvOpIsTerminator(searchOpCode)) {
                    uint32_t labelWordStart, labelWordCount, labelWordStride;
                    if (SpvHasLabels(searchOpCode, labelWordStart, labelWordCount, labelWordStride)) {
                        for (uint32_t j = 0; (j < labelWordCount) && ((labelWordStart + j * labelWordStride) < searchWordCount); j++) {
                            uint32_t searchLabelId = optimizedWords[searchWordIndex + labelWordStart + j * labelWordStride];
                            if (searchLabelId == instructionLabelId) {
                                foundBranchToThisBlock = true;
                                break;
                            }
                        }
                    }

                    break;
                }
            }

            // The preceding block did not have any reference to this block. Skip it.
            if (!foundBranchToThisBlock) {
                resultStack.emplace_back(optimizedWords[wordIndex + i]);
                continue;
            }

            // Copy the words.
            optimizedWords[wordIndex + newWordCount + 0] = optimizedWords[wordIndex + i + 0];
            optimizedWords[wordIndex + newWordCount + 1] = optimizedWords[wordIndex + i + 1];
            newWordCount += 2;
        }

        // Patch in the new word count.
        assert((optimizedWords[wordIndex] != UINT32_MAX) && "The instruction shouldn't be getting deleted from reducing the degree of the operands.");
        optimizedWords[wordIndex] = SpvOpPhi | (newWordCount << 16U);

        // Delete any of the remaining words.
        for (uint32_t i = newWordCount; i < wordCount; i++) {
            optimizedWords[wordIndex + i] = UINT32_MAX;
        }

        optimizerReduceResultDegrees(rContext, resultStack);

        return true;
    }

    static bool optimizerRunEvaluationPass(OptimizerContext &rContext) {
        if (!rContext.options.removeDeadCode) {
            return true;
        }

        uint32_t *optimizedWords = reinterpret_cast<uint32_t *>(rContext.optimizedData.data());
        uint32_t orderCount = uint32_t(rContext.shader.instructionOrder.size());
        for (uint32_t i = 0; i < orderCount; i++) {
            uint32_t instructionIndex = rContext.shader.instructionOrder[i];
            uint32_t wordIndex = rContext.shader.instructions[instructionIndex].wordIndex;

            // Instruction has been deleted.
            if (optimizedWords[wordIndex] == UINT32_MAX) {
                continue;
            }

            SpvOp opCode = SpvOp(optimizedWords[wordIndex] & 0xFFFFU);
            uint32_t wordCount = (optimizedWords[wordIndex] >> 16U) & 0xFFFFU;
            uint32_t patchedWordCount = wordCount;
            bool hasResult, hasType;
            SpvHasResultAndType(opCode, &hasResult, &hasType);

            if (hasResult) {
                if (opCode == SpvOpPhi) {
                    if (optimizerCompactPhi(instructionIndex, rContext)) {
                        patchedWordCount = (optimizedWords[wordIndex] >> 16U) & 0xFFFFU;
                    }
                    else {
                        return false;
                    }
                }

                // Check if any of the operands isn't a constant.
                bool allOperandsAreConstant = true;
                uint32_t operandWordStart, operandWordCount, operandWordStride, operandWordSkip;
                bool operandWordSkipString;
                if (SpvHasOperands(opCode, operandWordStart, operandWordCount, operandWordStride, operandWordSkip, operandWordSkipString)) {
                    uint32_t operandWordIndex = operandWordStart;
                    for (uint32_t j = 0; j < operandWordCount; j++) {
                        if (checkOperandWordSkip(wordIndex, optimizedWords, j, operandWordSkip, operandWordSkipString, operandWordIndex)) {
                            continue;
                        }

                        if (operandWordIndex >= patchedWordCount) {
                            break;
                        }

                        uint32_t operandId = optimizedWords[wordIndex + operandWordIndex];
                        assert((operandId != UINT32_MAX) && "An operand that's been deleted shouldn't be getting evaluated.");

                        // It shouldn't be possible for an operand to not be solved, but OpPhi can do so because previous blocks might've been deleted.
                        if ((opCode != SpvOpPhi) && (rContext.resolutions[operandId].type == Resolution::Type::Unknown)) {
                            fprintf(stderr, "Error in resolution of the operations. Operand %u was not solved.\n", operandId);
                            return false;
                        }

                        if (rContext.resolutions[operandId].type == Resolution::Type::Variable) {
                            allOperandsAreConstant = false;
                            break;
                        }

                        operandWordIndex += operandWordStride;
                    }
                }

                // The result can only be evaluated if all operands are constant.
                uint32_t resultId = optimizedWords[wordIndex + (hasType ? 2 : 1)];
                if (allOperandsAreConstant) {
                    optimizerEvaluateResult(resultId, rContext);
                }
                else {
                    rContext.resolutions[resultId].type = Resolution::Type::Variable;
                }
            }
            else if ((opCode == SpvOpBranchConditional) || (opCode == SpvOpSwitch)) {
                optimizerEvaluateTerminator(instructionIndex, rContext);
            }
        }

        return true;
    }

    static bool optimizerRemoveUnusedDecorations(OptimizerContext &rContext) {
        if (!rContext.options.removeDeadCode) {
            return true;
        }

        uint32_t *optimizedWords = reinterpret_cast<uint32_t *>(rContext.optimizedData.data());
        for (Decoration decoration : rContext.shader.decorations) {
            uint32_t wordIndex = rContext.shader.instructions[decoration.instructionIndex].wordIndex;
            uint32_t resultId = optimizedWords[wordIndex + 1];
            if (resultId == UINT32_MAX) {
                // This decoration has already been deleted.
                continue;
            }

            uint32_t resultInstructionIndex = rContext.shader.results[resultId].instructionIndex;
            uint32_t resultWordIndex = rContext.shader.instructions[resultInstructionIndex].wordIndex;

            // The result has been deleted, so we delete the decoration as well.
            if (optimizedWords[resultWordIndex] == UINT32_MAX) {
                optimizerEliminateInstruction(decoration.instructionIndex, rContext);
            }
        }

        return true;
    }

    static bool optimizerCompactPhis(OptimizerContext &rContext) {
        uint32_t *optimizedWords = reinterpret_cast<uint32_t *>(rContext.optimizedData.data());
        for (Phi phi : rContext.shader.phis) {
            uint32_t wordIndex = rContext.shader.instructions[phi.instructionIndex].wordIndex;
            if (optimizedWords[wordIndex] == UINT32_MAX) {
                // This operation has already been deleted.
                continue;
            }

            if (!optimizerCompactPhi(phi.instructionIndex, rContext)) {
                return false;
            }
        }

        return true;
    }

    static bool optimizerCompactData(OptimizerContext &rContext) {
        uint32_t *optimizedWords = reinterpret_cast<uint32_t *>(rContext.optimizedData.data());
        uint32_t optimizedWordCount = 0;
        uint32_t instructionCount = rContext.shader.instructions.size();

        // Copy the header.
        const uint32_t startingWordIndex = 5;
        for (uint32_t i = 0; i < startingWordIndex; i++) {
            optimizedWords[optimizedWordCount++] = optimizedWords[i];
        }

        // Write out all the words for all the instructions and skip any that were marked as deleted.
        for (uint32_t i = 0; i < instructionCount; i++) {
            uint32_t wordIndex = rContext.shader.instructions[i].wordIndex;

            // Instruction has been deleted.
            if (optimizedWords[wordIndex] == UINT32_MAX) {
                continue;
            }

            // Check if the instruction should be ignored.
            SpvOp opCode = SpvOp(optimizedWords[wordIndex] & 0xFFFFU);
            if (rContext.options.removeDeadCode && SpvIsIgnored(opCode)) {
                continue;
            }

            // Copy all the words of the instruction.
            uint32_t wordCount = (optimizedWords[wordIndex] >> 16U) & 0xFFFFU;
            for (uint32_t j = 0; j < wordCount; j++) {
                optimizedWords[optimizedWordCount++] = optimizedWords[wordIndex + j];
            }
        }

        rContext.optimizedData.resize(optimizedWordCount * sizeof(uint32_t));

        return true;
    }

    bool Optimizer::run(const Shader &pShader, const SpecConstant *pNewSpecConstants, uint32_t pNewSpecConstantCount, std::vector<uint8_t> &pOptimizedData, Options pOptions) {
        thread_local std::vector<uint32_t> instructionInDegrees;
        thread_local std::vector<uint32_t> instructionOutDegrees;
        thread_local std::vector<Resolution> resolutions;
        OptimizerContext context = { pShader, instructionInDegrees, instructionOutDegrees, resolutions, pOptimizedData, pOptions };
        if (!optimizerPrepareData(context)) {
            return false;
        }

        if (!optimizerPatchSpecializationConstants(pNewSpecConstants, pNewSpecConstantCount, context)) {
            return false;
        }

        if (!optimizerRunEvaluationPass(context)) {
            return false;
        }

        if (!optimizerRemoveUnusedDecorations(context)) {
            return false;
        }

        // FIXME: For some reason, it seems that based on the order of the resolution, OpPhis can be compacted
        // before all their preceding blocks have been evaluated in time whether they should be deleted or not.
        // This pass merely re-runs the compaction step as a safeguard to remove any stale references. There's
        // potential for further optimization if this is fixed properly.
        if (!optimizerCompactPhis(context)) {
            return false;
        }

        if (!optimizerCompactData(context)) {
            return false;
        }

        return true;
    }
};