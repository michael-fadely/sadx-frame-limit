#include "stdafx.h"

DataPointer(LARGE_INTEGER, PerformanceFrequency, 0x03D08538);
DataPointer(LARGE_INTEGER, PerformanceCounter, 0x03D08550);
DataPointer(int, FrameMultiplier, 0x0389D7DC);

static constexpr int native_frames_per_second = 60;

static bool enable_frame_limit = true;

static void __cdecl FrameLimit_r();

static Trampoline FrameLimit_t(0x007899E0, 0x007899E8, FrameLimit_r);

static void __cdecl FrameLimit_r()
{
	if (enable_frame_limit && QueryPerformanceFrequency(&PerformanceFrequency))
	{
		const auto frequency = FrameMultiplier * PerformanceFrequency.QuadPart / native_frames_per_second;
		LARGE_INTEGER counter;

		do
		{
			QueryPerformanceCounter(&counter);
		} while (counter.QuadPart - PerformanceCounter.QuadPart < frequency);

		PerformanceCounter = counter;
	}
	else
	{
		QueryPerformanceCounter(&PerformanceCounter);
	}
}

extern "C"
{
	__declspec(dllexport) ModInfo SADXModInfo = { ModLoaderVer };

	__declspec(dllexport) void __cdecl Init(const char* path, const HelperFunctions& helperFunctions)
	{
		QueryPerformanceCounter(&PerformanceCounter);

		// This patch changes 60.5 to 60, ensuring that the frame skip count at 0x3B11180
		// is updated properly.
		// FIXME: Unlike the default behavior, this can accumulate error and cause the some
		// straddling of the target-frame time which can in the end induce a constant stutter.
		WriteData<float>(reinterpret_cast<float*>(0x007DCE58), static_cast<float>(native_frames_per_second));
	}

#ifdef _DEBUG
	__declspec(dllexport) void __cdecl OnInput()
	{
		auto pad = ControllerPointers[0];
		if (pad && pad->HeldButtons & Buttons_R && pad->PressedButtons & Buttons_Y)
		{
			enable_frame_limit = !enable_frame_limit;
		}
	}
#endif
}
