#pragma once
#include <vector>

class EmbeddedShaders {
public:
    // DX11
    static inline const std::vector<uint8_t> shader_vertex_dx11{ ${SHADER_DX11_VERTEX} };

    static inline const std::vector<uint8_t> shader_fragment_dx11{ ${SHADER_DX11_FRAGMENT} };

    // DX12
    static inline const std::vector<uint8_t> shader_vertex_dx12{ ${SHADER_DX12_VERTEX} };

    static inline const std::vector<uint8_t> shader_fragment_dx12{ ${SHADER_DX12_FRAGMENT} };
};
