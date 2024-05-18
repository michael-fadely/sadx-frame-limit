#include "stdafx.h"

// Disable frame skip.
//
// Engine frame rate target adjustment still works, e.g. in 30 FPS cutscenes,
// but if the game lags, the game runs in slow motion. This mode provides the
// smoothest possible experience. No extra game ticks will execute.
#define FRAME_SKIP_MODE_DISABLE 0

// Error accumulation mode.
//
// Closest to original behavior, but creates an extremely terrible experience.
// Timing errors accumulate, and once they exceed a whole additional frame time
// the game will attempt to execute multiple game ticks in a single frame to
// recover. The vanilla game does this by targeting an integer frame rate just
// over 60 FPS, and reducing error by then assuming a target frame rate of 60.5.
#define FRAME_SKIP_MODE_ACCUMULATE_ERROR 1

// Hybrid single-tick lag compensation. Still doesn't provide a good experience.
//
// Unlike error accumulation mode, only lag which happens within a single game
// tick is considered for recovery tick execution. This means that at a target
// of 60 FPS, if a frame took 33.3 ms to execute, only one recovery tick is
// executed. However, if a frame took just under the threshold for an integer
// multiple of the target frame time, e.g. 33.0 ms, no recovery ticks will
// be executed.
#define FRAME_SKIP_MODE_DROP_ERROR 2

#define FRAME_SKIP_MODE FRAME_SKIP_MODE_DISABLE

DataPointer(LARGE_INTEGER, PerformanceFrequency, 0x03D08538);
DataPointer(LARGE_INTEGER, PerformanceCounter, 0x03D08550);
DataPointer(int, FrameMultiplier, 0x0389D7DC);

constexpr int native_frames_per_second = 60;

bool enable_frame_limit = true;

#if FRAME_SKIP_MODE != FRAME_SKIP_MODE_DISABLE
LARGE_INTEGER tick_start {};
#endif

#if FRAME_SKIP_MODE == FRAME_SKIP_MODE_ACCUMULATE_ERROR
LONGLONG tick_remainder = 0;
#endif

void __cdecl FrameLimit_r();
Trampoline FrameLimit_t(0x007899E0, 0x007899E8, FrameLimit_r);
void __cdecl FrameLimit_r()
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

int __cdecl calc_extra_tick_count_c()
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

#ifdef _DEBUG
	if (result != FrameMultiplier - 1)
	{
		PrintDebug("%d NO!!! Remainder: %lld\n", result, tick_remainder);
	}

	auto pad = ControllerPointers[0];
	if (pad && pad->PressedButtons & Buttons_D)
	{
		// 0.25ms
		tick_remainder += 250;
	}
#endif
#endif

	return result;
#endif
}

const void* calc_extra_tick_count_ret = (void*)0x00411DA3;
void __declspec(naked) calc_extra_tick_count_asm()
{
	__asm
	{
		call calc_extra_tick_count_c
		jmp calc_extra_tick_count_ret
	}
}

extern "C"
{
	__declspec(dllexport) ModInfo SADXModInfo = { ModLoaderVer, nullptr, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0 };

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
		// (use of calc_extra_tick_count_asm mostly invalidates this. keeping for reasons)
		WriteData<float>(reinterpret_cast<float*>(0x007DCE58), static_cast<float>(native_frames_per_second));

		WriteJump(reinterpret_cast<void*>(0x00411D1C), calc_extra_tick_count_asm);
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
