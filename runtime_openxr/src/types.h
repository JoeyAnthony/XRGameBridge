/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#pragma once
#include <stdint.h>

enum class GraphicsBackend {
    Uninitialized = 0,
    D3D11 = 1,
    D3D12 = 2,
    Vulkan = 3,
    OpenGL = 4
};

// Data types
struct GBVector2i {
    uint32_t x;
    uint32_t y;
};
