#include "stdafx.h"

#include <SADXModLoader.h>
#include <chrono>
#include "../sadx-mod-loader/libmodutils/Trampoline.h"

using namespace std;
using namespace chrono;

using FrameRatio = duration<double, ratio<1, 60>>;

static bool enable_frame_limit = true;
static auto frame_start = system_clock::now();
static auto frame_ratio = FrameRatio(1);
static int last_multi = 0;
static duration<double, milli> present_time = {};

static void __cdecl FrameLimit_r();
static void __cdecl SetFrameMultiplier_r(int a1);
static void __cdecl Direct3D_Present_r();

static Trampoline FrameLimit_t(0x007899E0, 0x007899E8, FrameLimit_r);
static Trampoline SetFrameMultiplier_t(0x007899A0, 0x007899A6, SetFrameMultiplier_r);
static Trampoline Direct3D_Present_t(0x0078BA30, 0x0078BA35, Direct3D_Present_r);

static void __cdecl FrameLimit_r()
{
	auto now = system_clock::now();

	if (enable_frame_limit && present_time < frame_ratio)
	{
		while ((now = system_clock::now()) - frame_start < frame_ratio)
		{
		}
	}

	frame_start = system_clock::now();
}

static void __cdecl SetFrameMultiplier_r(int a1)
{
	if (a1 != last_multi)
	{
		*(int*)0x0389D7DC = a1;
		last_multi = a1;
		frame_ratio = FrameRatio(a1);
	}
}

static void __cdecl Direct3D_Present_r()
{
	auto original = (decltype(Direct3D_Present_r)*)Direct3D_Present_t.Target();

	// This is done to avoid vsync issues.
	auto start = system_clock::now();
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
