#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "engine/assets/asset_reader.hpp"
#include "engine/core/status.hpp"
#include "engine/core/types.hpp"

namespace gameengine::editor {

struct AssetRecord final {
    std::string relative_path{};
    assets::AssetKind kind = assets::AssetKind::mesh;
    core::u64 byte_size = 0;
    core::u64 asset_id = 0;
    core::u64 content_hash = 0;
    core::u32 primary_count = 0;
    core::u32 secondary_count = 0;
    core::u32 tertiary_count = 0;
    bool valid = false;
    std::string detail{};
    std::string error{};
};

class AssetCatalog final {
public:
    static constexpr core::u32 max_assets = 512U;
    static constexpr core::u32 invalid_index = 0xffffffffU;

    [[nodiscard]] core::Status refresh(const std::filesystem::path& project_root) noexcept;

    [[nodiscard]] std::span<const AssetRecord> records() const noexcept { return records_; }
    [[nodiscard]] core::u32 selected_index() const noexcept { return selected_index_; }
    [[nodiscard]] const AssetRecord* selected() const noexcept;
    [[nodiscard]] core::Status select(core::u32 index) noexcept;

    [[nodiscard]] static std::string_view kind_name(assets::AssetKind kind) noexcept;

private:
    std::vector<AssetRecord> records_{};
    core::u32 selected_index_ = invalid_index;
};

} // namespace gameengine::editor
