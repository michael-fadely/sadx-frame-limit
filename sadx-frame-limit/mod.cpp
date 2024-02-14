#include "stdafx.h"

#define FRAME_SKIP_MODE_DISABLE 0
#define FRAME_SKIP_MODE_ACCUMULATE_ERROR 1
#define FRAME_SKIP_MODE_DROP_ERROR 2

#define FRAME_SKIP_MODE FRAME_SKIP_MODE_ACCUMULATE_ERROR

DataPointer(LARGE_INTEGER, PerformanceFrequency, 0x03D08538);
DataPointer(LARGE_INTEGER, PerformanceCounter, 0x03D08550);
DataPointer(int, FrameMultiplier, 0x0389D7DC);

static constexpr int native_frames_per_second = 60;

static bool enable_frame_limit = true;

#if FRAME_SKIP_MODE != FRAME_SKIP_MODE_DISABLE
static LARGE_INTEGER tick_start {};
#endif

#if FRAME_SKIP_MODE == FRAME_SKIP_MODE_ACCUMULATE_ERROR
static LONGLONG tick_remainder = 0;
#endif

static void __cdecl FrameLimit_r();
static Trampoline FrameLimit_t(0x007899E0, 0x007899E8, FrameLimit_r);
static void __cdecl FrameLimit_r()
{
	if (enable_frame_limit && QueryPerformanceFrequency(&PerformanceFrequency))
	{
		const LONGLONG frequency = FrameMultiplier * PerformanceFrequency.QuadPart / native_frames_per_second;
		LARGE_INTEGER counter;
		LONGLONG elapsed;

		do
		{
			QueryPerformanceCounter(&counter);
			elapsed = counter.QuadPart - PerformanceCounter.QuadPart;
		} while (elapsed < frequency);

		PerformanceCounter = counter;
	}
	else
	{
		QueryPerformanceCounter(&PerformanceCounter);
	}
}

static int __cdecl lmao_c()
{
#if FRAME_SKIP_MODE == FRAME_SKIP_MODE_DISABLE
	return std::max<int>(0, FrameMultiplier - 1);
#else
	constexpr int to_usec = 1'000'000;
	const LONGLONG frame_time_usec = to_usec / native_frames_per_second;

	LARGE_INTEGER counter {};
	QueryPerformanceCounter(&counter);

	auto delta = ((counter.QuadPart - tick_start.QuadPart) * to_usec) / PerformanceFrequency.QuadPart;
	tick_start = counter;

	// FRAME_SKIP_MODE != FRAME_SKIP_MODE_DROP_ERROR
#if FRAME_SKIP_MODE == FRAME_SKIP_MODE_ACCUMULATE_ERROR
	delta += tick_remainder;
#endif

	const auto frames_to_rerun = std::max<int>(static_cast<int>(delta / frame_time_usec), 0);
	const auto result = std::max(frames_to_rerun - 1, 0);

#if FRAME_SKIP_MODE == FRAME_SKIP_MODE_ACCUMULATE_ERROR
	const auto remainder = delta - (frame_time_usec * frames_to_rerun);

	tick_remainder = remainder;

	if (result != FrameMultiplier - 1)
	{
		PrintDebug("FUCK!!! Remainder: %lld\n", tick_remainder);
	}

	auto pad = ControllerPointers[0];
	if (pad && pad->PressedButtons & Buttons_D)
	{
		// 0.25ms
		tick_remainder += 250;
	}
#endif

	return result;
#endif
}

static const void* lmao_ret = (void*)0x00411DA3;
static void __declspec(naked) lmao_asm()
{
	__asm
	{
		call lmao_c
		jmp lmao_ret
	}
}

extern "C"
{
	__declspec(dllexport) ModInfo SADXModInfo = { ModLoaderVer };

	__declspec(dllexport) void __cdecl Init(const char* path, const HelperFunctions& helperFunctions)
	{
		QueryPerformanceCounter(&PerformanceCounter);
#if FRAME_SKIP_MODE != FRAME_SKIP_MODE_DISABLE
		tick_start = PerformanceCounter;
#endif

		// This patch changes 60.5 to 60, ensuring that the frame skip count at 0x3B11180
		// is updated properly.
		// FIXME: Unlike the default behavior, this can accumulate error and cause some
		// straddling of the target-frame time which can in the end induce a constant stutter.
		WriteData<float>(reinterpret_cast<float*>(0x007DCE58), static_cast<float>(native_frames_per_second));

		WriteJump((void*)0x00411D1C, lmao_asm);
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
