#pragma once
#pragma once
#include <openxr/openxr.h>

enum class CommResult {
	SUCCESS,
	RUNTIME_NOT_FOUND,
	FUNCTION_NOT_FOUND
};

struct CommBackBufferDescription {
	uint32_t width;
	uint32_t height;
	int64_t format;
    void* windowHandle;
};

// Runtime functions
namespace rcomm {
CommResult InitializeCommInterface();
	void DeinitializeCommInterface();
	XrResult xrgbGetReleasedBufferHandle(XrSession session, uint64_t* resourceHandle);
} // namespace rcomm

// plugin functions
namespace pcomm {
CommResult InitializeCommInterface();
void DeinitializeCommInterface();
CommBackBufferDescription xrgbGetBackbufferDescription();
}
