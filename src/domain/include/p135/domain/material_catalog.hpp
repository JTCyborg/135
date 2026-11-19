#pragma once

#include "p135/domain/magnetic_product.hpp"

#include <cstddef>
#include <string>
#include <unordered_map>

namespace p135::domain {

/// Missing loss data means core-loss calculations are NOT_VERIFIED, never zero-loss.
[[nodiscard]] Result<void> validate(const CoreMaterial& material);

class CoreMaterialCatalog {
public:
    [[nodiscard]] Result<void> add(CoreMaterial material);
    [[nodiscard]] const CoreMaterial* find(const std::string& id) const noexcept;
    [[nodiscard]] std::size_t size() const noexcept { return materials_.size(); }

private:
    std::unordered_map<std::string, CoreMaterial> materials_;
};

} // namespace p135::domain
