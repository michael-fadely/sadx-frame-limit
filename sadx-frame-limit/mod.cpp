#include "stdafx.h"

#include <SADXModLoader.h>
#include <chrono>
#include <thread>
#include "../sadx-mod-loader/libmodutils/Trampoline.h"

using namespace std::chrono;

using FrameRatio = duration<double, std::ratio<1, 60>>;

static bool enable_frame_limit = true;
static auto frame_start = system_clock::now();
static auto frame_ratio = FrameRatio(1);
static int last_multi = 0;
static duration<double, std::milli> present_time = {};

static const auto frame_portion_ms = duration_cast<milliseconds>(frame_ratio) - milliseconds(1);

static void __cdecl FrameLimit_r();
static void __cdecl SetFrameMultiplier_r(int a1);
static void __cdecl Direct3D_Present_r();

static Trampoline FrameLimit_t(0x007899E0, 0x007899E8, FrameLimit_r);
static Trampoline SetFrameMultiplier_t(0x007899A0, 0x007899A6, SetFrameMultiplier_r);
static Trampoline Direct3D_Present_t(0x0078BA30, 0x0078BA35, Direct3D_Present_r);

static void __cdecl FrameLimit_r()
{
	if (enable_frame_limit && present_time < frame_ratio)
	{
		auto now = system_clock::now();
		const milliseconds delta = duration_cast<milliseconds>(now - frame_start);

		if (delta < frame_ratio)
		{
			// sleep for a portion of the frame time to free up cpu time
			std::this_thread::sleep_for(frame_portion_ms - delta);

			while ((now = system_clock::now()) - frame_start < frame_ratio)
			{
				// spin for the remainder of the time
			}
		}
	}

	frame_start = system_clock::now();
}

static void __cdecl SetFrameMultiplier_r(int a1)
{
	if (a1 != last_multi)
	{
		*reinterpret_cast<int*>(0x0389D7DC) = a1;
		last_multi = a1;
		frame_ratio = FrameRatio(a1);
	}
}

static void __cdecl Direct3D_Present_r()
{
	const auto original = static_cast<decltype(Direct3D_Present_r)*>(Direct3D_Present_t.Target());

	// This is done to avoid vsync issues.
	const auto start = system_clock::now();
	original();
	present_time = system_clock::now() - start;
}

extern "C"
{
	__declspec(dllexport) ModInfo SADXModInfo = { ModLoaderVer };

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
