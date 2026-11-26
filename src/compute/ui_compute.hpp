#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <span>

#include "kernetile/core/primitives.hpp"
#include "kernetile/atlas/atlas_types.hpp"

namespace kernetile::compute {

struct Tile {
    uint32_t x;
    uint32_t y;
    uint32_t offset;
    uint32_t count;
};
static_assert(sizeof(Tile) == 16);

struct Frame {
    uint32_t width;
    uint32_t height;
    std::span<const core::UiPrimitive> primitives;
    std::span<const uint32_t> indices;
    std::span<const Tile> tiles;
    std::span<const core::ClipRect> clips;
    std::span<const uint8_t> atlas;
    uint32_t atlas_width;
    uint32_t atlas_height;
    std::span<const atlas::RectU32> atlas_updates;
    bool mica_enabled{false};
    std::array<uint32_t, 16> mica_grid{};
    std::span<const uint8_t> color_atlas{};
    uint32_t color_atlas_width{0};
    uint32_t color_atlas_height{0};
    std::span<const atlas::RectU32> color_atlas_updates{};
};

class Backend {
public:
    virtual ~Backend() = default;
    virtual bool ready() const noexcept = 0;
    virtual bool submit(const Frame& frame) = 0;
};

std::unique_ptr<Backend> create(void* window, void* display, bool prefer_warp = false);

} // namespace kernetile::compute
