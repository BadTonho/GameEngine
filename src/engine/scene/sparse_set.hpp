#pragma once

#include <cstddef>
#include <span>
#include <utility>
#include <vector>

#include "engine/core/types.hpp"
#include "engine/scene/scene.hpp"

namespace gameengine::scene {

template <typename Component>
class SparseSet final {
public:
    [[nodiscard]] bool contains(Entity entity) const noexcept
    {
        if (!entity.valid() || entity.index() >= sparse_.size()) {
            return false;
        }
        const core::u32 dense_index = sparse_[entity.index()];
        return dense_index != invalid_dense_index && dense_index < dense_entities_.size() &&
               dense_entities_[dense_index] == entity;
    }

    template <typename... Arguments>
    Component* emplace(Entity entity, Arguments&&... arguments)
    {
        if (!entity.valid() || contains(entity)) {
            return nullptr;
        }
        if (entity.index() >= sparse_.size()) {
            sparse_.resize(static_cast<core::usize>(entity.index()) + 1U, invalid_dense_index);
        }
        dense_entities_.push_back(entity);
        dense_components_.emplace_back(std::forward<Arguments>(arguments)...);
        sparse_[entity.index()] = static_cast<core::u32>(dense_entities_.size() - 1U);
        return &dense_components_.back();
    }

    bool remove(Entity entity) noexcept
    {
        if (!contains(entity)) {
            return false;
        }
        const core::u32 dense_index = sparse_[entity.index()];
        const core::u32 last_index = static_cast<core::u32>(dense_entities_.size() - 1U);
        if (dense_index != last_index) {
            dense_entities_[dense_index] = dense_entities_[last_index];
            dense_components_[dense_index] = std::move(dense_components_[last_index]);
            sparse_[dense_entities_[dense_index].index()] = dense_index;
        }
        dense_entities_.pop_back();
        dense_components_.pop_back();
        sparse_[entity.index()] = invalid_dense_index;
        return true;
    }

    [[nodiscard]] Component* get(Entity entity) noexcept
    {
        return contains(entity) ? &dense_components_[sparse_[entity.index()]] : nullptr;
    }

    [[nodiscard]] const Component* get(Entity entity) const noexcept
    {
        return contains(entity) ? &dense_components_[sparse_[entity.index()]] : nullptr;
    }

    [[nodiscard]] std::span<const Entity> entities() const noexcept { return dense_entities_; }
    [[nodiscard]] std::span<const Component> components() const noexcept { return dense_components_; }
    [[nodiscard]] core::usize size() const noexcept { return dense_components_.size(); }
    [[nodiscard]] core::usize memory_usage_bytes() const noexcept
    {
        return sparse_.capacity() * sizeof(core::u32) +
            dense_entities_.capacity() * sizeof(Entity) +
            dense_components_.capacity() * sizeof(Component);
    }

    void clear() noexcept
    {
        sparse_.clear();
        dense_entities_.clear();
        dense_components_.clear();
    }

private:
    static constexpr core::u32 invalid_dense_index = 0xffffffffU;
    std::vector<core::u32> sparse_;
    std::vector<Entity> dense_entities_;
    std::vector<Component> dense_components_;
};

} // namespace gameengine::scene
