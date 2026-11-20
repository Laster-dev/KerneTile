#pragma once

// ============================================================================
// KerneTile: Modern Compute Shader Driven 2D UI Rendering Engine
// Single Include Header for Clients and Embedders
// ============================================================================

#include "kernetile/core/types.hpp"
#include "kernetile/core/primitives.hpp"
#include "kernetile/core/tile_header.hpp"

#include "kernetile/math/vec.hpp"
#include "kernetile/math/rect.hpp"
#include "kernetile/math/color.hpp"
#include "kernetile/math/color_space.hpp"

#include "kernetile/hal/hal_types.hpp"
#include "kernetile/hal/hal.hpp"

#include "kernetile/tile/tile_grid.hpp"
#include "kernetile/tile/tile_binner.hpp"
#include "kernetile/tile/dirty_tracker.hpp"

#include "kernetile/ring/vring_types.hpp"
#include "kernetile/ring/lockfree_ring.hpp"
#include "kernetile/ring/submission_channel.hpp"

#include "kernetile/atlas/atlas_types.hpp"
#include "kernetile/atlas/shelf_packer.hpp"
#include "kernetile/atlas/dynamic_atlas.hpp"

#include "kernetile/text/text_types.hpp"
#include "kernetile/text/font_engine.hpp"
#include "kernetile/text/stem_darkening.hpp"
#include "kernetile/text/text_streamer.hpp"

#include "kernetile/fx/blur_types.hpp"
#include "kernetile/fx/dual_kawase_blur.hpp"
#include "kernetile/fx/clip_stack.hpp"

#include "kernetile/input/pointer_events.hpp"
#include "kernetile/input/pointer_pipeline.hpp"
#include "kernetile/input/key_codes.hpp"
#include "kernetile/input/ime_state_machine.hpp"

#include "kernetile/platform/presentation_sync.hpp"

#include "kernetile/cpu/cpu_rasterizer.hpp"
#include "kernetile/cpu/image_compare.hpp"

#include "kernetile/renderer.hpp"
