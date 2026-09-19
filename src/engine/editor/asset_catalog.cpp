#include "engine/editor/asset_catalog.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>

namespace gameengine::editor {

namespace {

constexpr core::u64 max_asset_bytes = 64ULL * 1024ULL * 1024ULL;

[[nodiscard]] core::Status invalid() noexcept
{
    return core::Status{core::ErrorCode::invalid_argument};
}

[[nodiscard]] bool supported_extension(std::string_view extension,
                                       assets::AssetKind& kind) noexcept
{
    if (extension == ".gemesh") {
        kind = assets::AssetKind::mesh;
        return true;
    }
    if (extension == ".getex") {
        kind = assets::AssetKind::texture;
        return true;
    }
    if (extension == ".gemat") {
        kind = assets::AssetKind::material;
        return true;
    }
    if (extension == ".gescene") {
        kind = assets::AssetKind::scene;
        return true;
    }
    return false;
}

void set_invalid_record(AssetRecord& record, std::string_view message) noexcept
{
    record.valid = false;
    record.detail = "INVALID";
    record.error.assign(message);
}

void inspect_bytes(AssetRecord& record, std::span<const std::byte> bytes) noexcept
{
    assets::AssetView asset;
    core::Status status;
    switch (record.kind) {
    case assets::AssetKind::mesh: {
        assets::MeshView view;
        status = assets::read_mesh(bytes, view);
        if (status) {
            asset = view.asset;
            record.primary_count = view.vertex_count;
            record.secondary_count = view.index_count;
            record.tertiary_count = view.submesh_count;
            record.detail = "V" + std::to_string(view.vertex_count) + " I" +
                            std::to_string(view.index_count) + " S" +
                            std::to_string(view.submesh_count);
        }
        break;
    }
    case assets::AssetKind::texture: {
        assets::TextureView view;
        status = assets::read_texture(bytes, view);
        if (status) {
            asset = view.asset;
            record.primary_count = view.width;
            record.secondary_count = view.height;
            record.tertiary_count = view.mip_count;
            record.detail = "W" + std::to_string(view.width) + " H" +
                            std::to_string(view.height) + " M" +
                            std::to_string(view.mip_count);
        }
        break;
    }
    case assets::AssetKind::material: {
        assets::MaterialView view;
        status = assets::read_material(bytes, view);
        if (status) {
            asset = view.asset;
            record.detail = "PBR";
        }
        break;
    }
    case assets::AssetKind::scene: {
        assets::SceneView view;
        status = assets::read_scene(bytes, view);
        if (status) {
            asset = view.asset;
            record.primary_count = view.extended ? view.entity_count : view.instance_count;
            record.detail = (view.extended ? "E" : "I") +
                            std::to_string(record.primary_count);
        }
        break;
    }
    case assets::AssetKind::package:
        status = invalid();
        break;
    }

    if (!status) {
        set_invalid_record(record, "asset validation failed");
        return;
    }
    record.valid = true;
    record.asset_id = asset.asset_id;
    record.content_hash = asset.content_hash;
}

[[nodiscard]] bool normalized_relative_path(const std::filesystem::path& root,
                                            const std::filesystem::path& path,
                                            std::string& output) noexcept
{
    const std::filesystem::path relative = path.lexically_relative(root);
    if (relative.empty() || relative.is_absolute()) {
        return false;
    }
    for (const auto& component : relative) {
        if (component == ".." || component == ".") {
            return false;
        }
    }
    output = relative.generic_string();
    return !output.empty();
}

} // namespace

core::Status AssetCatalog::refresh(const std::filesystem::path& project_root) noexcept
{
    std::error_code error;
    const std::filesystem::path root =
        std::filesystem::absolute(project_root, error).lexically_normal();
    if (error || !std::filesystem::is_directory(root, error) || error) {
        return invalid();
    }

    std::string selected_path;
    if (const AssetRecord* current = selected(); current != nullptr) {
        selected_path = current->relative_path;
    }

    std::vector<AssetRecord> next;
    next.reserve(std::min<std::size_t>(max_assets, 64U));
    std::filesystem::recursive_directory_iterator iterator(
        root, std::filesystem::directory_options::skip_permission_denied, error);
    const std::filesystem::recursive_directory_iterator end;
    for (; iterator != end && !error; iterator.increment(error)) {
        const std::filesystem::directory_entry& entry = *iterator;
        std::error_code entry_error;
        if (entry.is_symlink(entry_error)) {
            iterator.disable_recursion_pending();
            continue;
        }
        if (entry_error || !entry.is_regular_file(entry_error) || entry_error) {
            continue;
        }

        std::string extension = entry.path().extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](char value) {
            return static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
        });
        assets::AssetKind kind = assets::AssetKind::mesh;
        if (!supported_extension(extension, kind)) {
            continue;
        }
        if (next.size() >= max_assets) {
            return invalid();
        }

        AssetRecord record;
        record.kind = kind;
        if (!normalized_relative_path(root, entry.path(), record.relative_path)) {
            continue;
        }
        const std::uintmax_t file_size = entry.file_size(entry_error);
        if (entry_error || file_size > max_asset_bytes ||
            file_size > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max())) {
            record.byte_size = file_size > max_asset_bytes ? max_asset_bytes : 0;
            set_invalid_record(record, "asset file size is invalid");
            next.push_back(std::move(record));
            continue;
        }
        record.byte_size = static_cast<core::u64>(file_size);
        std::ifstream file(entry.path(), std::ios::binary);
        if (!file) {
            set_invalid_record(record, "asset file cannot be opened");
            next.push_back(std::move(record));
            continue;
        }
        std::vector<std::byte> bytes(static_cast<std::size_t>(file_size));
        if (file_size != 0U) {
            file.read(reinterpret_cast<char*>(bytes.data()),
                      static_cast<std::streamsize>(bytes.size()));
        }
        if (!file.good() && !file.eof()) {
            set_invalid_record(record, "asset file cannot be read");
        } else {
            inspect_bytes(record, bytes);
        }
        next.push_back(std::move(record));
    }
    if (error) {
        return invalid();
    }

    std::sort(next.begin(), next.end(), [](const AssetRecord& left, const AssetRecord& right) {
        return left.relative_path < right.relative_path;
    });
    records_ = std::move(next);
    selected_index_ = invalid_index;
    if (!selected_path.empty()) {
        for (core::u32 index = 0; index < records_.size(); ++index) {
            if (records_[index].relative_path == selected_path) {
                selected_index_ = index;
                break;
            }
        }
    }
    return {};
}

const AssetRecord* AssetCatalog::selected() const noexcept
{
    return selected_index_ < records_.size() ? &records_[selected_index_] : nullptr;
}

core::Status AssetCatalog::select(core::u32 index) noexcept
{
    if (index >= records_.size()) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    selected_index_ = index;
    return {};
}

std::string_view AssetCatalog::kind_name(assets::AssetKind kind) noexcept
{
    switch (kind) {
    case assets::AssetKind::mesh:
        return "GEMESH";
    case assets::AssetKind::texture:
        return "GETEX";
    case assets::AssetKind::material:
        return "GEMAT";
    case assets::AssetKind::scene:
        return "GESCENE";
    case assets::AssetKind::package:
        return "PACKAGE";
    }
    return "UNKNOWN";
}

} // namespace gameengine::editor
