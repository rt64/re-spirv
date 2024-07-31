//
// re-spirv
//

#include "re-spirv.h"

#include <cassert>

#include "spirv/unified1/spirv.hpp"

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

        return true;
    }

    bool Optimizer::empty() const {
        return spirvWords == nullptr;
    }

    const std::vector<SpecConstant> &Optimizer::getDefaultSpecConstants() const {
        return defaultSpecConstants;
    }

    bool Optimizer::run(const SpecConstant *specConstants, uint32_t specConstantCount, std::vector<uint8_t> &optimizedData) const {
        assert(!empty());

        optimizedData.resize(spirvWordCount * sizeof(uint32_t));
        memcpy(optimizedData.data(), spirvWords, optimizedData.size());
        return true;
    }
};