//
// re-spirv
//

#include <vector>

namespace respv {
    struct SpecConstant {
        uint32_t specId = 0;

        union {
            bool boolValue;
            int32_t intValue;
            uint32_t uintValue;
            float floatValue;
        };
    };

    struct Optimizer {
        const uint32_t *spirvWords = nullptr;
        size_t spirvWordCount = 0;
        std::vector<SpecConstant> defaultSpecConstants;

        // Default empty constructor.
        Optimizer();

        // Parses the SPIR-V during construction. Data must remain available during the lifetime of the object.
        Optimizer(const void *data, size_t size);

        // Data must remain available during the lifetime of the object.
        bool parse(const void *data, size_t size);

        // Check whether the optimizer has parsed any SPIR-V data.
        bool empty() const;

        // Return the default specialization constants that were encountered.
        const std::vector<SpecConstant> &getDefaultSpecConstants() const;

        // Run the optimizer with the specified values for specialization constants and return the optimized data.
        bool run(const SpecConstant *specConstants, uint32_t specConstantCount, std::vector<uint8_t> &optimizedData) const;
    };
};