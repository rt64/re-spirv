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

    struct Optimizer {
        const uint32_t *spirvWords = nullptr;
        size_t spirvWordCount = 0;
        std::vector<SpecConstant> specConstants; // Size is however many spec constants were found.
        std::vector<uint32_t> specConstantsTargetIds; // Size is however many spec constants were found.
        std::vector<uint32_t> specIdToConstantIndex; // Size will fit whatever was the biggest spec Id that was found.
        std::vector<uint32_t> resultIndices; // Size is the SPIR-V Id Bound.
        std::vector<uint32_t> decoratorIndices; // Size is however many decorations were found.
        bool filledData = false;

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
};